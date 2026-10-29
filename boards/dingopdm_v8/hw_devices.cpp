#include "hw_devices.h"
#include "port_pwm.h"

Profet pf[NUM_OUTPUTS] = {
    Profet(1, ProfetModel::BTS7002_1EPP, LINE_PF1_IN, LINE_UNUSED, LINE_UNUSED, AnalogChannel::IS1, &PWMD3, &pwm3Cfg, PwmChannel::Ch1),
    Profet(2, ProfetModel::BTS7002_1EPP, LINE_PF2_IN, LINE_UNUSED, LINE_UNUSED, AnalogChannel::IS2, &PWMD4, &pwm4Cfg, PwmChannel::Ch1),
    Profet(3, ProfetModel::BTS7002_1EPP, LINE_PF3_IN, LINE_UNUSED, LINE_UNUSED, AnalogChannel::IS3, &PWMD5, &pwm5Cfg, PwmChannel::Ch1),
    Profet(4, ProfetModel::BTS7002_1EPP, LINE_PF4_IN, LINE_UNUSED, LINE_UNUSED, AnalogChannel::IS4, &PWMD9, &pwm9Cfg, PwmChannel::Ch1),
    Profet(5, ProfetModel::BTS7002_1EPP, LINE_PF5_IN, LINE_UNUSED, LINE_UNUSED, AnalogChannel::IS5, &PWMD10, &pwm10Cfg, PwmChannel::Ch1),
    Profet(6, ProfetModel::BTS7002_1EPP, LINE_PF6_IN, LINE_UNUSED, LINE_UNUSED, AnalogChannel::IS6, &PWMD11, &pwm11Cfg, PwmChannel::Ch1),
    Profet(7, ProfetModel::BTS7002_1EPP, LINE_PF7_IN, LINE_UNUSED, LINE_UNUSED, AnalogChannel::IS7, &PWMD12, &pwm12Cfg, PwmChannel::Ch1),
    Profet(8, ProfetModel::BTS7002_1EPP, LINE_PF8_IN, LINE_UNUSED, LINE_UNUSED, AnalogChannel::IS8, &PWMD13, &pwm13Cfg, PwmChannel::Ch1),
    Profet(9, ProfetModel::BTS7002_1EPP, LINE_PF9_IN, LINE_UNUSED, LINE_UNUSED, AnalogChannel::IS9, &PWMD14, &pwm14Cfg, PwmChannel::Ch1),
    Profet(10, ProfetModel::BTS7002_1EPP, LINE_PF10_IN, LINE_UNUSED, LINE_UNUSED, AnalogChannel::IS10, &PWMD15, &pwm15Cfg, PwmChannel::Ch1),
    Profet(11, ProfetModel::BTS7002_1EPP, LINE_PF11_IN, LINE_UNUSED, LINE_UNUSED, AnalogChannel::IS11, &PWMD13, &pwm13Cfg, PwmChannel::Ch1),
    Profet(12, ProfetModel::BTS7002_1EPP, LINE_PF12_IN, LINE_UNUSED, LINE_UNUSED, AnalogChannel::IS12, &PWMD13, &pwm13Cfg, PwmChannel::Ch1),
    Profet(13, ProfetModel::BTS7002_1EPP, LINE_PF13_IN, LINE_UNUSED, LINE_UNUSED, AnalogChannel::IS13, &PWMD13, &pwm13Cfg, PwmChannel::Ch1),
    Profet(14, ProfetModel::BTS7002_1EPP, LINE_PF14_IN, LINE_UNUSED, LINE_UNUSED, AnalogChannel::IS14, &PWMD13, &pwm13Cfg, PwmChannel::Ch1),
    Profet(15, ProfetModel::BTS7002_1EPP, LINE_PF15_IN, LINE_UNUSED, LINE_UNUSED, AnalogChannel::IS15, &PWMD13, &pwm13Cfg, PwmChannel::Ch1),
    Profet(16, ProfetModel::BTS7002_1EPP, LINE_PF16_IN, LINE_UNUSED, LINE_UNUSED, AnalogChannel::IS16, &PWMD13, &pwm13Cfg, PwmChannel::Ch1)};

Analog_Input analogIn[NUM_ANALOG_INPUTS] = {
    Analog_Input(AnalogChannel::AIN1),
    Analog_Input(AnalogChannel::AIN2),
    Analog_Input(AnalogChannel::AIN3),
    Analog_Input(AnalogChannel::AIN4)};

Digital_Input digIn[NUM_DIG_INPUTS] = {
    Digital_Input(LINE_DI1),
    Digital_Input(LINE_DI2),
    Digital_Input(LINE_DI3),
    Digital_Input(LINE_DI4),
    Digital_Input(LINE_DI5),
    Digital_Input(LINE_DI6)};    

Led statusLed = Led(LINE_LED_STATUS);
Led errorLed = Led(LINE_LED_ERROR);

MCP9808 tempSensor(I2CD2, MCP9808_I2CADDR_DEFAULT);
