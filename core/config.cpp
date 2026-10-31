#include "config.h"
#include "error.h"
#include "crc.h"
#include "param_protocol.h"
#include "config_ext.h"

//Try to read config from internal flash first
//If it fails, try external memory
//If that fails, return false
#if !HAS_EXT_MEMORY
// CRC goes on the next 8 byte boundary, G4 flash programs 8 byte double words that can only be written once
#define CONFIG_CRC_OFFSET (CONFIG_FLASH_OFFSET + ((sizeof(DeviceConfig) + 7U) & ~7U))

// Config + CRC must fit in the reserved 2KB flash pages
static_assert(CONFIG_CRC_OFFSET - CONFIG_FLASH_OFFSET + sizeof(uint32_t) <= CONFIG_NUM_SECTORS * 2048U, "DeviceConfig too big for reserved config flash");
#endif

bool ReadConfig(){

    if(HAS_EXT_MEMORY)
    {
        return ReadConfigExt();
    }

    #if !HAS_EXT_MEMORY
    // DeviceConfig is ~5KB, too big for the 1KB main() stack, borrow the staging buffer (unused until param thread starts)
    DeviceConfig& tempConfig = stConfigTemp;

    flash_error_t err = flashRead(CONFIG_FLASH, CONFIG_FLASH_OFFSET,
                                  sizeof(DeviceConfig), (uint8_t*)&tempConfig);
    if (err != FLASH_NO_ERROR)
    {
        return ReadConfigExt();
    }

    if (tempConfig.stDevice.nConfigVersion != CONFIG_VERSION)
    {
        return ReadConfigExt();
    }

    uint32_t storedCrc;
    err = flashRead(CONFIG_FLASH, CONFIG_CRC_OFFSET,
                    sizeof(uint32_t), (uint8_t*)&storedCrc);
    if (err != FLASH_NO_ERROR)
    {
        return ReadConfigExt();
    }

    if (CalculateCRC32(&tempConfig, sizeof(DeviceConfig)) != storedCrc)
    {
        return ReadConfigExt();
    }

    stConfig = tempConfig;
    return true;
    #else
    return false;
    #endif
}

//Try to write config from internal flash first
//If it fails, try external memory
//If that fails, return false 
bool WriteConfig(){

    if(HAS_EXT_MEMORY)
    {
        return WriteConfigExt();
    }

    #if !HAS_EXT_MEMORY
    stConfig.stDevice.nConfigVersion = CONFIG_VERSION;

    flash_error_t err = FLASH_NO_ERROR;
    for (uint32_t i = 0; i < CONFIG_NUM_SECTORS; i++)
    {
        err = flashStartEraseSector(CONFIG_FLASH, CONFIG_SECTOR + i);
        if (err != FLASH_NO_ERROR)
        {
            return WriteConfigExt();
        }

        err = flashWaitErase(CONFIG_FLASH);
        if (err != FLASH_NO_ERROR)
        {
            return WriteConfigExt();
        }
    }

    err = flashProgram(CONFIG_FLASH, CONFIG_FLASH_OFFSET,
                       sizeof(DeviceConfig), (const uint8_t*)&stConfig);
    if (err != FLASH_NO_ERROR)
    {
        return WriteConfigExt();
    }

    uint32_t crc = CalculateCRC32(&stConfig, sizeof(DeviceConfig));
    err = flashProgram(CONFIG_FLASH, CONFIG_CRC_OFFSET,
                       sizeof(uint32_t), (const uint8_t*)&crc);

    return (err == FLASH_NO_ERROR);
    #else
    return false;
    #endif
}

void InitConfig()
{
    #if !HAS_EXT_MEMORY
    eflStart(&EFLD1, NULL);
    #endif

    if (!ReadConfig())
    {
        SetAllDefaultParams();
        if (!WriteConfig())
            Error::SetFatalError(FatalErrorType::ErrConfig, MsgSrc::Config);
    }

}