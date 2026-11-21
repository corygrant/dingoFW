#pragma once

#include <cstdint>
#include "port.h"

typedef struct {
    uint8_t nId;
    uint8_t nData[8];
    uint8_t nLength;
    uint8_t nChecksum;
} stLinFrame;

extern bool bOn;

extern uint8_t nData1;
extern uint8_t nData2;
extern bool bMoving;
extern uint8_t nWiperPos;

void InitLin(void);
bool LinRxIsActive(void);