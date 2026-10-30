#include "port.h"
#include "mcu_utils.h"
#include "hw_devices.h"

/*
 FDCAN kernel clock = PCLK1 = 170MHz, 17 tq per bit (Seg1 = 13, Seg2 = 3), 82.4% sample point
 Prescaler = 170MHz / (17 * bitrate). Subtract '1' for register values
 Classic CAN only, non-matching frames rejected so the filters in can.cpp decide what's received
*/
#define FDCAN_CLASSIC_CONFIG(nPrescaler)                                                       \
{                                                                                              \
    OPMODE_CAN,                                                                                \
    FDCAN_CONFIG_NBTP_NSJW(2) | FDCAN_CONFIG_NBTP_NBRP((nPrescaler) - 1) |                     \
    FDCAN_CONFIG_NBTP_NTSEG1(12) | FDCAN_CONFIG_NBTP_NTSEG2(2),                                \
    0,                                                                                         \
    0,                                                                                         \
    0,                                                                                         \
    FDCAN_CONFIG_RXGFC_ANFS_REJECT | FDCAN_CONFIG_RXGFC_ANFE_REJECT                           \
}

static const CANConfig canConfig1000 = FDCAN_CLASSIC_CONFIG(10);
static const CANConfig canConfig500 = FDCAN_CLASSIC_CONFIG(20);
static const CANConfig canConfig250 = FDCAN_CLASSIC_CONFIG(40);
static const CANConfig canConfig125 = FDCAN_CLASSIC_CONFIG(80);
static const CANConfig canConfig100 = FDCAN_CLASSIC_CONFIG(100);

const CANConfig& GetCanConfig(CanBitrate bitrate) {
    switch(bitrate) {
        case CanBitrate::Bitrate_1000K:
            return canConfig1000;
        case CanBitrate::Bitrate_500K:
            return canConfig500;
        case CanBitrate::Bitrate_250K:
            return canConfig250;
        case CanBitrate::Bitrate_125K:
            return canConfig125;
        case CanBitrate::Bitrate_100K:
            return canConfig100;
        default:
            return canConfig500;
    }
    return canConfig500;
}

volatile adcsample_t adc1_samples[ADC1_NUM_CHANNELS] = {0};
//0  = ADC1_IN1  - Out13_IS
//1  = ADC1_IN2  - Out12_IS
//2  = ADC1_IN3  - Out10_IS
//3  = ADC1_IN4  - OUT15_IS
//4  = ADC1_IN12 - OUT11_IS
//5  = ADC1_IN16 - TempSensor
//6  = ADC1_IN18 - VRefInt

volatile adcsample_t adc2_samples[ADC2_NUM_CHANNELS] = {0};
//0  = ADC2_IN3  - OUT14_IS
//1  = ADC2_IN4  - OUT16_IS
//2  = ADC2_IN5  - OUT5_IS
//3  = ADC2_IN6  - OUT1_IS
//4  = ADC2_IN7  - OUT8_IS
//5  = ADC2_IN8  - OUT6_IS
//6  = ADC2_IN9  - OUT3_IS
//7  = ADC2_IN11 - OUT9_IS
//8  = ADC2_IN12 - OUT7_IS
//9  = ADC2_IN13 - OUT4_IS
//10 = ADC2_IN17 - OUT2_IS

volatile adcsample_t adc3_samples[ADC3_NUM_CHANNELS] = {0};
//0  = ADC3_IN9  - BATT_SENSE
//1  = ADC3_IN13 - OPAMP3 - AIN1 

volatile adcsample_t adc4_samples[ADC4_NUM_CHANNELS] = {0};
//0  = ADC4_IN17 - OPAMP6 - AIN3

volatile adcsample_t adc5_samples[ADC5_NUM_CHANNELS] = {0};
//0  = ADC5_IN3  - OPAMP5 - AIN2
//1  = ADC5_IN5  - OPAMP4 - AIN4

/*
 ADC clock = AHB/4 = 42.5MHz
 92.5 cycle sample time = ~2.2us, closest match to dingoPDM_v7 (15 cycles @ 9MHz = ~1.7us)
 Temp sensor and VRefInt need >= 5us on the G4, 247.5 cycles = ~5.8us
*/
static const ADCConversionGroup adc1_cfg = {
    .circular = true,
    .num_channels = ADC1_NUM_CHANNELS,
    .end_cb = NULL,
    .error_cb = NULL,
    .cfgr = ADC_CFGR_OVRMOD | ADC_CFGR_RES_12BITS | ADC_CFGR_CONT,
    .cfgr2 = 0,
    .tr1 = ADC_TR_DISABLED,
    .tr2 = ADC_TR_DISABLED,
    .tr3 = ADC_TR_DISABLED,
    .awd2cr = 0,
    .awd3cr = 0,
    .smpr = {ADC_SMPR1_SMP_AN1(ADC_SMPR_SMP_92P5) |
             ADC_SMPR1_SMP_AN2(ADC_SMPR_SMP_92P5) |
             ADC_SMPR1_SMP_AN3(ADC_SMPR_SMP_92P5) |
             ADC_SMPR1_SMP_AN4(ADC_SMPR_SMP_92P5),
             ADC_SMPR2_SMP_AN12(ADC_SMPR_SMP_92P5) |
             ADC_SMPR2_SMP_AN16(ADC_SMPR_SMP_247P5) |
             ADC_SMPR2_SMP_AN18(ADC_SMPR_SMP_247P5)},
    .sqr = {ADC_SQR1_SQ1_N(ADC_CHANNEL_IN1) |
            ADC_SQR1_SQ2_N(ADC_CHANNEL_IN2) |
            ADC_SQR1_SQ3_N(ADC_CHANNEL_IN3) |
            ADC_SQR1_SQ4_N(ADC_CHANNEL_IN4),
            ADC_SQR2_SQ5_N(ADC_CHANNEL_IN12) |
            ADC_SQR2_SQ6_N(ADC_CHANNEL_IN16) |
            ADC_SQR2_SQ7_N(ADC_CHANNEL_IN18),
            0,
            0}};

static const ADCConversionGroup adc2_cfg = {
    .circular = true,
    .num_channels = ADC2_NUM_CHANNELS,
    .end_cb = NULL,
    .error_cb = NULL,
    .cfgr = ADC_CFGR_OVRMOD | ADC_CFGR_RES_12BITS | ADC_CFGR_CONT,
    .cfgr2 = 0,
    .tr1 = ADC_TR_DISABLED,
    .tr2 = ADC_TR_DISABLED,
    .tr3 = ADC_TR_DISABLED,
    .awd2cr = 0,
    .awd3cr = 0,
    .smpr = {ADC_SMPR1_SMP_AN3(ADC_SMPR_SMP_92P5) |
             ADC_SMPR1_SMP_AN4(ADC_SMPR_SMP_92P5) |
             ADC_SMPR1_SMP_AN5(ADC_SMPR_SMP_92P5) |
             ADC_SMPR1_SMP_AN6(ADC_SMPR_SMP_92P5) |
             ADC_SMPR1_SMP_AN7(ADC_SMPR_SMP_92P5) |
             ADC_SMPR1_SMP_AN8(ADC_SMPR_SMP_92P5) |
             ADC_SMPR1_SMP_AN9(ADC_SMPR_SMP_92P5),
             ADC_SMPR2_SMP_AN11(ADC_SMPR_SMP_92P5) |
             ADC_SMPR2_SMP_AN12(ADC_SMPR_SMP_92P5) |
             ADC_SMPR2_SMP_AN13(ADC_SMPR_SMP_92P5) |
             ADC_SMPR2_SMP_AN17(ADC_SMPR_SMP_92P5)},
    .sqr = {ADC_SQR1_SQ1_N(ADC_CHANNEL_IN3) |
            ADC_SQR1_SQ2_N(ADC_CHANNEL_IN4) |
            ADC_SQR1_SQ3_N(ADC_CHANNEL_IN5) |
            ADC_SQR1_SQ4_N(ADC_CHANNEL_IN6),
            ADC_SQR2_SQ5_N(ADC_CHANNEL_IN7) |
            ADC_SQR2_SQ6_N(ADC_CHANNEL_IN8) |
            ADC_SQR2_SQ7_N(ADC_CHANNEL_IN9) |
            ADC_SQR2_SQ8_N(ADC_CHANNEL_IN11) |
            ADC_SQR2_SQ9_N(ADC_CHANNEL_IN12),
            ADC_SQR3_SQ10_N(ADC_CHANNEL_IN13) |
            ADC_SQR3_SQ11_N(ADC_CHANNEL_IN17),
            0}};

static const ADCConversionGroup adc3_cfg = {
    .circular = true,
    .num_channels = ADC3_NUM_CHANNELS,
    .end_cb = NULL,
    .error_cb = NULL,
    .cfgr = ADC_CFGR_OVRMOD | ADC_CFGR_RES_12BITS | ADC_CFGR_CONT,
    .cfgr2 = 0,
    .tr1 = ADC_TR_DISABLED,
    .tr2 = ADC_TR_DISABLED,
    .tr3 = ADC_TR_DISABLED,
    .awd2cr = 0,
    .awd3cr = 0,
    .smpr = {ADC_SMPR1_SMP_AN9(ADC_SMPR_SMP_92P5),
             ADC_SMPR2_SMP_AN13(ADC_SMPR_SMP_92P5)},
    .sqr = {ADC_SQR1_SQ1_N(ADC_CHANNEL_IN9) |
            ADC_SQR1_SQ2_N(ADC_CHANNEL_IN13),
            0,
            0,
            0}};

static const ADCConversionGroup adc4_cfg = {
    .circular = true,
    .num_channels = ADC4_NUM_CHANNELS,
    .end_cb = NULL,
    .error_cb = NULL,
    .cfgr = ADC_CFGR_OVRMOD | ADC_CFGR_RES_12BITS | ADC_CFGR_CONT,
    .cfgr2 = 0,
    .tr1 = ADC_TR_DISABLED,
    .tr2 = ADC_TR_DISABLED,
    .tr3 = ADC_TR_DISABLED,
    .awd2cr = 0,
    .awd3cr = 0,
    .smpr = {0,
             ADC_SMPR2_SMP_AN17(ADC_SMPR_SMP_92P5)},
    .sqr = {ADC_SQR1_SQ1_N(ADC_CHANNEL_IN17),
            0,
            0,
            0}};

static const ADCConversionGroup adc5_cfg = {
    .circular = true,
    .num_channels = ADC5_NUM_CHANNELS,
    .end_cb = NULL,
    .error_cb = NULL,
    .cfgr = ADC_CFGR_OVRMOD | ADC_CFGR_RES_12BITS | ADC_CFGR_CONT,
    .cfgr2 = 0,
    .tr1 = ADC_TR_DISABLED,
    .tr2 = ADC_TR_DISABLED,
    .tr3 = ADC_TR_DISABLED,
    .awd2cr = 0,
    .awd3cr = 0,
    .smpr = {ADC_SMPR1_SMP_AN3(ADC_SMPR_SMP_92P5) |
             ADC_SMPR1_SMP_AN5(ADC_SMPR_SMP_92P5),
             0},
    .sqr = {ADC_SQR1_SQ1_N(ADC_CHANNEL_IN3) |
            ADC_SQR1_SQ2_N(ADC_CHANNEL_IN5),
            0,
            0,
            0}};

/*
 OPAMP3-6 as unity gain buffers for AIN1-4
 VMSEL = 11 (follower), OPAMPINTEN routes the output to the ADC instead of the VOUT pin
 VOUT pins (PB1, PB12, PA8, PB11) are used for other functions so must stay disconnected
 Factory trim used (USERTRIM = 0), SYSCFG clock enabled by the HAL
*/
#define OPAMP_FOLLOWER (OPAMP_CSR_OPAMPxEN | OPAMP_CSR_VMSEL_0 | OPAMP_CSR_VMSEL_1 | OPAMP_CSR_OPAMPINTEN)

static void InitOpamps()
{
    OPAMP3->CSR = OPAMP_FOLLOWER | (1U << OPAMP_CSR_VPSEL_Pos); // VINP1 = PB13 - AIN1 -> ADC3_IN13
    OPAMP4->CSR = OPAMP_FOLLOWER | (1U << OPAMP_CSR_VPSEL_Pos); // VINP1 = PD11 - AIN4 -> ADC5_IN5
    OPAMP5->CSR = OPAMP_FOLLOWER | (0U << OPAMP_CSR_VPSEL_Pos); // VINP0 = PB14 - AIN2 -> ADC5_IN3
    OPAMP6->CSR = OPAMP_FOLLOWER | (1U << OPAMP_CSR_VPSEL_Pos); // VINP1 = PD9  - AIN3 -> ADC4_IN17
}

static void DeInitOpamps()
{
    OPAMP3->CSR = 0;
    OPAMP4->CSR = 0;
    OPAMP5->CSR = 0;
    OPAMP6->CSR = 0;
}

msg_t InitAdc()
{
    msg_t ret;

    InitOpamps();

    ret = adcStart(&ADCD1, NULL);
    if(ret != HAL_RET_SUCCESS)
        return ret;

    adcSTM32EnableTS(&ADCD1); // Enable temperature sensor
    adcSTM32EnableVREF(&ADCD1); // Enable vref

    adcStartConversion(&ADCD1, &adc1_cfg, const_cast<adcsample_t*>(adc1_samples), ADC1_BUF_DEPTH);

    ret = adcStart(&ADCD2, NULL);
    if(ret != HAL_RET_SUCCESS)
        return ret;

    adcStartConversion(&ADCD2, &adc2_cfg, const_cast<adcsample_t*>(adc2_samples), ADC2_BUF_DEPTH);

    ret = adcStart(&ADCD3, NULL);
    if(ret != HAL_RET_SUCCESS)
        return ret;

    adcStartConversion(&ADCD3, &adc3_cfg, const_cast<adcsample_t*>(adc3_samples), ADC3_BUF_DEPTH);

    ret = adcStart(&ADCD4, NULL);
    if(ret != HAL_RET_SUCCESS)
        return ret;

    adcStartConversion(&ADCD4, &adc4_cfg, const_cast<adcsample_t*>(adc4_samples), ADC4_BUF_DEPTH);

    ret = adcStart(&ADCD5, NULL);
    if(ret != HAL_RET_SUCCESS)
        return ret;

    adcStartConversion(&ADCD5, &adc5_cfg, const_cast<adcsample_t*>(adc5_samples), ADC5_BUF_DEPTH);

    return HAL_RET_SUCCESS;
}

void DeInitAdc()
{
    adcStopConversion(&ADCD1);
    adcStop(&ADCD1);
    adcStopConversion(&ADCD2);
    adcStop(&ADCD2);
    adcStopConversion(&ADCD3);
    adcStop(&ADCD3);
    adcStopConversion(&ADCD4);
    adcStop(&ADCD4);
    adcStopConversion(&ADCD5);
    adcStop(&ADCD5);

    DeInitOpamps();
}

uint16_t GetAdcRaw(AnalogChannel channel)
{
    switch (channel)
    {
    case AnalogChannel::IS1:
        return adc2_samples[3];
    case AnalogChannel::IS2:
        return adc2_samples[10];
    case AnalogChannel::IS3:
        return adc2_samples[6];
    case AnalogChannel::IS4:
        return adc2_samples[9];
    case AnalogChannel::IS5:
        return adc2_samples[2];
    case AnalogChannel::IS6:
        return adc2_samples[5];
    case AnalogChannel::IS7:
        return adc2_samples[8];
    case AnalogChannel::IS8:
        return adc2_samples[4];
    case AnalogChannel::IS9:
        return adc2_samples[7];
    case AnalogChannel::IS10:
        return adc1_samples[2];
    case AnalogChannel::IS11:
        return adc1_samples[4];
    case AnalogChannel::IS12:
        return adc1_samples[1];
    case AnalogChannel::IS13:
        return adc1_samples[0];
    case AnalogChannel::IS14:
        return adc2_samples[0];
    case AnalogChannel::IS15:
        return adc1_samples[3];
    case AnalogChannel::IS16:
        return adc2_samples[1];
    case AnalogChannel::AIN1:
        return adc3_samples[1];
    case AnalogChannel::AIN2:
        return adc5_samples[0];
    case AnalogChannel::AIN3:
        return adc4_samples[0];
    case AnalogChannel::AIN4:
        return adc5_samples[1];
    case AnalogChannel::BattVolt:
        return adc3_samples[0];
    case AnalogChannel::TempSensor:
        return adc1_samples[5];
    case AnalogChannel::VRefInt:
        return adc1_samples[6];
    default:
        return 0; // Invalid channel
    }
}

float AdcToVolts(adcsample_t raw)
{
    // MCU vRef = measured VDDA
    // 4095 counts full scale
    float mcuVolts = (GetVDDA() / 4095) * raw;

    const float rUpper = 4700;
    const float rLower = 10000;

    return mcuVolts * ((rUpper + rLower) / rLower);
}

float GetAdcVolts(AnalogChannel channel)
{
    return AdcToVolts(GetAdcRaw(channel));
}

float GetBattVolt()
{
    // MCU vRef = 3.3v
    // 4095 counts full scale
    float mcuVolts = (GetVDDA() / 4095) * GetAdcRaw(AnalogChannel::BattVolt);

    const float rUpper = 47000;
    const float rLower = 4700;

    return mcuVolts * ((rUpper + rLower) / rLower);
}

float GetTemperature()
{
    return (STM32_TS_CAL1_TEMP + ((float)(GetAdcRaw(AnalogChannel::TempSensor) - STM32_TS_CAL1) / (STM32_TS_CAL2 - STM32_TS_CAL1)) * (STM32_TS_CAL2_TEMP - STM32_TS_CAL1_TEMP));
}

float GetVDDA()
{
    uint16_t raw = GetAdcRaw(AnalogChannel::VRefInt);
    if (raw == 0)
        return 3.3; // No conversion yet, assume nominal

    return ((float)STM32_VREF_INT_CAL / (float)raw) * STM32_CAL_VDDA;
}
#if HAS_NEOPIXELS
void UpdateNeopixels()
{
    // Pixel assignments not defined yet, just push whatever is set
    intNeoPixels.Update();
}
#endif
