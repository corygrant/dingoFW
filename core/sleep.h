#pragma once

#include "ch.h"
#include "hal.h"
#include "config.h"
#include "enums.h"

// Sleep management functions extracted from pdm.cpp

bool CheckEnterSleep();
void EnterSleep();
void EnableLineEventWithPull(ioline_t line, InputPull pull);

// What woke the device from its last sleep, read at boot
#define WAKE_SRC_CAN        0x01
#define WAKE_SRC_DIG_IN     0x02
#define WAKE_SRC_USB        0x04
#define WAKE_SRC_OTHER_LINE 0x08
#define WAKE_SRC_INTERRUPT  0x10 // a pending interrupt that is not a wake line
#define WAKE_SRC_FROM_SLEEP 0x80 // this boot followed a sleep, 0 = power on or reset

void CaptureWakeSource();
uint8_t GetLastWakeSource();