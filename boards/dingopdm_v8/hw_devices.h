#pragma once

#include "port.h"
#include "profet.h"
#include "analog_input.h"
#include "digital_input.h"
#include "led.h"
#include "hardware/mcp9808.h"
#include "functions/neopixels.h"

extern Profet pf[NUM_OUTPUTS];
extern Analog_Input analogIn[NUM_ANALOG_INPUTS];
extern Digital_Input digIn[NUM_DIG_INPUTS];
extern Led statusLed;
extern Led errorLed;
extern MCP9808 tempSensor;
extern NeoPixels neoPixels;