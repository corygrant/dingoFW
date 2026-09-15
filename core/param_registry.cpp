#include "param_registry.h"
#include "params.h"
#include "crc.h"
#include <cstring>
#include <bit>

//=============================================================================
// Parameter Table - Lives in flash (.rodata)
// Pointers to stConfig members are resolved at link time
//=============================================================================
const ParamInfo stParams[] = {
    // Device Config (0x0000)
    DEVICE_CONFIG_PARAMS(),

    // Outputs (0x1000+)
    #if NUM_OUTPUTS > 0
    ALL_OUTPUT_PARAMS,
    #endif

    #if NUM_DIG_INPUTS > 0
    // Digital Inputs (0x1200+)
    ALL_DIGITAL_INPUT_PARAMS,
    #endif

    // CAN Inputs (0x1300+)
    ALL_CAN_INPUT_PARAMS,

    // Virtual Inputs (0x1400+)
    ALL_VIRTUAL_INPUT_PARAMS,

    // Conditions (0x1500+)
    ALL_CONDITION_PARAMS,

    // Counters (0x1600+)
    ALL_COUNTER_PARAMS,

    // Flashers (0x1700+)
    ALL_FLASHER_PARAMS,

    #if HAS_STARTER_DISABLE > 0
    // Starter (0x1800)
    STARTER_PARAMS(),
    ALL_STARTER_DISABLE_PARAMS,
    #endif

    #if HAS_WIPERS > 0
    // Wiper (0x1900)
    WIPER_PARAMS(),
    WIPER_SPEED_MAP_PARAMS(),
    WIPER_INTERMIT_PARAMS(),
    #endif

    // CAN Outputs (0x2000+)
    ALL_CAN_OUTPUT_PARAMS,

    #if NUM_DIG_OUTPUTS > 0
    // Digital Outputs (0x2100+)
    ALL_DIGITAL_OUTPUT_PARAMS,
    #endif

    #if NUM_ANALOG_INPUTS > 0
    // Analog Inputs (0x2200+)
    ALL_ANALOG_INPUT_PARAMS,
    #endif

    #if NUM_KEYPADS > 0
    // Keypads (0x3000+)
    ALL_KEYPAD_PARAMS,

    // Keypad Buttons (0x3100+)
    ALL_KEYPAD_BUTTON_PARAMS,

    // Keypad Dials (0x3200+)
    ALL_KEYPAD_DIAL_PARAMS,
    #endif
};

const uint16_t NUM_PARAMS = sizeof(stParams) / sizeof(stParams[0]);

//=============================================================================
// WriteAll received-param tracking (full WriteAll only, see param_registry.h)
//=============================================================================

static uint8_t writeReceivedMask[(NUM_PARAMS + 7) / 8];

void ResetWriteReceivedMask() {
    memset(writeReceivedMask, 0, sizeof(writeReceivedMask));
}

void MarkParamReceived(const ParamInfo* param) {
    if (param == nullptr) return;
    size_t i = param - stParams; // valid: param always points into stParams[]
    writeReceivedMask[i / 8] |= static_cast<uint8_t>(1u << (i % 8));
}

bool IsParamReceived(uint16_t i) {
    return (writeReceivedMask[i / 8] & (1u << (i % 8))) != 0;
}

uint16_t CountReceivedParams() {
    uint16_t count = 0;
    for (size_t i = 0; i < sizeof(writeReceivedMask); i++)
        count += static_cast<uint16_t>(std::popcount(writeReceivedMask[i]));
    return count;
}

//=============================================================================
// Parameter Access Functions
//=============================================================================

const ParamInfo* FindParam(uint16_t index, uint8_t subindex) {
    for (uint16_t i = 0; i < NUM_PARAMS; i++) {
        if (stParams[i].nIndex == index &&
            stParams[i].nSubIndex == subindex) {
            return &stParams[i];
        }
    }
    return nullptr;
}

uint32_t ReadParam(const ParamInfo* param, bool temp)
{
    if(param == nullptr || param->pVal == nullptr || param->pTempVal == nullptr)
        return 0;

    void* targetPtr = temp ? param->pTempVal : param->pVal;

    switch (param->eType) {
        case ParamType::Bool:
            return *static_cast<bool*>(targetPtr) ? 1 : 0;
        case ParamType::UInt8:
        case ParamType::Enum:
            return *static_cast<uint8_t*>(targetPtr);
        case ParamType::Int8:
            return static_cast<uint32_t>(*static_cast<int8_t*>(targetPtr));
        case ParamType::UInt16:
            return *static_cast<uint16_t*>(targetPtr);
        case ParamType::Int16:
            return static_cast<uint32_t>(*static_cast<int16_t*>(targetPtr));
        case ParamType::UInt32:
            return *static_cast<uint32_t*>(targetPtr);
        case ParamType::Float: {
            uint32_t raw;
            memcpy(&raw, targetPtr, sizeof(uint32_t));
            return raw;
        }
        case ParamType::Int32:
            return static_cast<uint32_t>(*static_cast<int32_t*>(targetPtr));
        default:
            return 0;
    }
}

bool WriteParam(const ParamInfo* param, uint32_t value, bool temp)
{
    if(param == nullptr || param->pVal == nullptr || param->pTempVal == nullptr)
        return false;

    // Range validation using native wire-format uint32_t limits
    switch (param->eType) {
        case ParamType::Float: {
            float fVal, fMin, fMax;
            memcpy(&fVal, &value,           sizeof(float));
            memcpy(&fMin, &param->nMinVal,  sizeof(float));
            memcpy(&fMax, &param->nMaxVal,  sizeof(float));
            if (fVal < fMin || fVal > fMax) return false;
            break;
        }
        case ParamType::Int8:
        case ParamType::Int16:
        case ParamType::Int32: {
            int32_t v   = static_cast<int32_t>(value);
            int32_t min = static_cast<int32_t>(param->nMinVal);
            int32_t max = static_cast<int32_t>(param->nMaxVal);
            if (v < min || v > max) return false;
            break;
        }
        default: // UInt8, UInt16, UInt32, Bool, Enum
            if (value < param->nMinVal || value > param->nMaxVal) return false;
            break;
    }

    void* targetPtr = temp ? param->pTempVal : param->pVal;

    switch (param->eType) {
        case ParamType::Bool:
            *static_cast<bool*>(targetPtr) = (value != 0);
            break;
        case ParamType::UInt8:
        case ParamType::Enum:
            *static_cast<uint8_t*>(targetPtr) = static_cast<uint8_t>(value);
            break;
        case ParamType::Int8:
            *static_cast<int8_t*>(targetPtr) = static_cast<int8_t>(value);
            break;
        case ParamType::UInt16:
            *static_cast<uint16_t*>(targetPtr) = static_cast<uint16_t>(value);
            break;
        case ParamType::Int16:
            *static_cast<int16_t*>(targetPtr) = static_cast<int16_t>(value);
            break;
        case ParamType::UInt32:
            *static_cast<uint32_t*>(targetPtr) = value;
            break;
        case ParamType::Float:
            memcpy(targetPtr, &value, sizeof(uint32_t));
            break;
        case ParamType::Int32:
            *static_cast<int32_t*>(targetPtr) = static_cast<int32_t>(value);
            break;
        default:
            return false;
    }
    return true;
}

bool IsDefaultValue(const ParamInfo* param)
{
    if(param == nullptr || param->pVal == nullptr)
        return false;

    return ReadParam(param) == param->nDefaultVal;
}

uint32_t CalcParamsCrc(bool temp)
{
    uint32_t crc = 0xFFFFFFFF;

    for (uint16_t i = 0; i < NUM_PARAMS; i++) {
        uint32_t value = ReadParam(&stParams[i], temp);
        // Little-endian wire encoding, matching EncodeParamRsp's data8[4..7] layout,
        // independent of host byte order.
        uint8_t bytes[4] = {
            static_cast<uint8_t>(value & 0xFF),
            static_cast<uint8_t>((value >> 8) & 0xFF),
            static_cast<uint8_t>((value >> 16) & 0xFF),
            static_cast<uint8_t>((value >> 24) & 0xFF)
        };
        crc = CalculateCRC32Partial(bytes, 4, crc);
    }

    return ~crc;
}
