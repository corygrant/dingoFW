#include "lin.h"
#include "port.h"
#include <cstring>

#define RX_TIMEOUT_MS 500

uint8_t nCounter = 0;
uint8_t nData1 = 0;
uint8_t nData2 = 0;
bool bMoving = false;
uint8_t nWiperPos = 0;

bool bSlowSpeed = true;
bool bFastSpeed = false;
bool bIntMode = false;
bool bSwipeMode = false;
uint8_t nIntModeSpeed = 0;

uint32_t nLastRxTime = 0;

static UARTConfig lin_config = {
    .txend1_cb = NULL,
    .txend2_cb = NULL,
    .rxend_cb = NULL,
    .rxchar_cb = NULL,
    .rxerr_cb = NULL,
    .timeout_cb = NULL,
    .timeout = 100,
    .speed = 19200,
    .cr1 = 0,
    .cr2 = USART_CR2_LINEN | USART_CR2_RTOEN,
    .cr3 = 0
};

static void LinSendBreak(void)
{
    UARTD2.usart->RQR |= USART_RQR_SBKRQ;
    
    // Wait for break completion
    while(UARTD2.usart->ISR & USART_ISR_SBKF) {
        chThdSleepMicroseconds(10);
    }

}

// Calculate protected ID with parity bits
static uint8_t LinCalculateProtectedId(uint8_t id) {
    uint8_t p0 = (id >> 0) ^ (id >> 1) ^ (id >> 2) ^ (id >> 4);
    uint8_t p1 = ~((id >> 1) ^ (id >> 3) ^ (id >> 4) ^ (id >> 5));
    
    return id | ((p0 & 1) << 6) | ((p1 & 1) << 7);
}

uint8_t calculateLIN2xChecksum(uint8_t protected_id, uint8_t *data, size_t length) {
    uint16_t sum = protected_id;
    
    for(uint8_t i = 0; i < length; i++) {
        sum += data[i];
    }
    
    // RFC 1071 style checksum (Internet checksum)
    while(sum >> 8) {
        sum = (sum & 0xFF) + (sum >> 8);
    }
    
    return ~sum & 0xFF;
}

uint8_t calculateLIN1xChecksum(uint8_t *data, size_t length) {
    uint16_t sum = 0;  // Start with 0, don't include protected_id
    
    for(uint8_t i = 0; i < length; i++) {
        sum += data[i];
    }
    
    // RFC 1071 style checksum (Internet checksum)
    while(sum >> 8) {
        sum = (sum & 0xFF) + (sum >> 8);
    }
    
    return ~sum & 0xFF;
}

// Send complete LIN frame (master)
msg_t LinSendFrame(stLinFrame *frame, bool bChecksum) {
    systime_t timeout = chTimeMS2I(10);
    size_t size = 0;

    // 1. Send break field
    LinSendBreak();

    // 2. Send sync byte
    uint8_t sync = 0x55;
    size = 1;
    if(uartSendFullTimeout(&UARTD2, &size, &sync, timeout) != MSG_OK) {
        return MSG_TIMEOUT;
    }

    uint8_t txData[10];
    txData[0] = LinCalculateProtectedId(frame->nId);
    txData[1] = frame->nData[0]; // First data byte
    txData[2] = frame->nData[1]; // Second data byte
    txData[3] = frame->nData[2]; // Third data byte
    txData[4] = frame->nData[3]; // Fourth data byte
    txData[5] = frame->nData[4]; // Fifth data byte
    txData[6] = frame->nData[5]; // Sixth data byte
    txData[7] = frame->nData[6]; // Seventh data byte
    txData[8] = frame->nData[7]; // Eighth data byte
    if(bChecksum) {
        frame->nChecksum = calculateLIN1xChecksum(frame->nData, frame->nLength);
        txData[1 + frame->nLength] = frame->nChecksum;
        size = 2 + frame->nLength; // Sync + ID + Data + Checksum
    } else {
        size = 1 + frame->nLength; // Sync + ID + Data
    }

    if(uartSendFullTimeout(&UARTD2, &size, &txData, timeout) != MSG_OK) {
        return MSG_TIMEOUT;
    }

    return MSG_OK;
}

msg_t LinGetResponse(uint8_t id, stLinFrame *rxFrame) {
    stLinFrame frame;
    uint8_t rxData[10];

    rxFrame->nData[0] = 0;
    rxFrame->nData[1] = 0;
    rxFrame->nData[2] = 0;
    rxFrame->nData[3] = 0;
    rxFrame->nData[4] = 0;
    rxFrame->nData[5] = 0;
    rxFrame->nData[6] = 0;
    rxFrame->nData[7] = 0;

    // Send diagnostic frame
    frame.nId = 0xB1;
    frame.nLength = 0;

    if (LinSendFrame(&frame, false) == MSG_OK) {
        // Frame sent successfully
    }

    chThdSleepMicroseconds(100);
    
    // Receive response (data + checksum)
    size_t response_length = 6; // Data + Checksum
    if (uartReceiveTimeout(&UARTD2, &response_length, rxData, chTimeMS2I(50)) != MSG_OK) {
        return MSG_TIMEOUT;
    }
    
    // Copy data and verify checksum
    memcpy(rxFrame->nData, rxData, rxFrame->nLength);
    uint8_t expected_checksum = calculateLIN1xChecksum(rxFrame->nData, rxFrame->nLength);

    if (expected_checksum != rxData[rxFrame->nLength]) {
        return MSG_RESET; // Checksum error
    }

    nLastRxTime = SYS_TIME;
    
    rxFrame->nId = id;
    rxFrame->nChecksum = rxData[rxFrame->nLength];
    return MSG_OK;
}


// LIN master thread
static THD_WORKING_AREA(lin_master_wa, 512);
static THD_FUNCTION(lin_master_thread, arg) {
    (void)arg;
    chRegSetThreadName("lin_master");

    stLinFrame frame;

    nCounter = 0;

    while (true) {

        nCounter++;
        if (nCounter > 15)    
            nCounter = 0;

        // Send periodic frame
        frame.nId = 0xF0;
        frame.nLength = 5;
        frame.nData[0] = 0x30 + (nCounter * 0x0F);;
        frame.nData[1] = nData1;// 0x08;// (bSlowSpeed << 7) | (bFastSpeed << 6) | (bIntMode << 5) | (bSwipeMode << 4) | (nIntModeSpeed & 0x0F);
        frame.nData[2] = nData2;//0x01;
        frame.nData[3] = 0x00;
        frame.nData[4] = 0x00;

        if (LinSendFrame(&frame, true) == MSG_OK) {
            // Frame sent successfully

        }

        chThdSleepMilliseconds(120);

        frame.nId = 0x00;
        frame.nLength = 5;
        frame.nData[0] = 0x00;
        frame.nData[1] = 0x00;
        frame.nData[2] = 0x00;
        frame.nData[3] = 0x00;
        frame.nData[4] = 0x00;

        frame.nChecksum = 0x00;

        LinGetResponse(0xB1, &frame);

        bMoving = ((frame.nData[3] >> 5 ) & 0x01) == 1;
        nWiperPos = frame.nData[1];

        chThdSleepMilliseconds(120);
    }
}

bool LinRxIsActive(void)
{
    return (SYS_TIME - nLastRxTime) < RX_TIMEOUT_MS;
}

// Put LIN transceiver to sleep
void LinSleep(void) {
    uartStop(&UARTD2);
    palClearLine(LINE_LIN_STANDBY); // Put TJA1029T to sleep
}

// Wake up LIN transceiver
void LinWakeup(void) {
    palSetLine(LINE_LIN_STANDBY);    // Wake up TJA1029T
    uartStart(&UARTD2, &lin_config); // Start UART driver
}

void InitLin(void) {
    LinWakeup();

    // Start LIN master thread
    chThdCreateStatic(lin_master_wa, sizeof(lin_master_wa), NORMALPRIO, lin_master_thread, nullptr);
    
}