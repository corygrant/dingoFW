// Host check of the LIN protocol helpers in comms/lin_proto.h
//   g++ -std=c++17 -Icore -Icomms tests/lin_check.cpp -o lin_check && ./lin_check
#include <cassert>
#include <cstdio>
#include "lin_proto.h"

int main()
{
    // Protected IDs
    assert(LinPid(0x00) == 0x80);
    assert(LinPid(0x01) == 0xC1);
    assert(LinPid(0x30) == 0xF0);
    assert(LinPid(0x31) == 0xB1);
    assert(LinPid(0x3C) == 0x3C);
    assert(LinPid(0x3D) == 0x7D);
    assert(LinPid(0xF0) == 0xF0); // Already a PID: masked to 0x30 first
    assert(LinPid(0xB1) == 0xB1);

    // Checksum example from the LIN 2.x spec: PID 0x4A, data 55 93 E5 -> E6
    const uint8_t aData[] = {0x55, 0x93, 0xE5};
    assert(LinCalcChecksum(0x4A, aData, 3, LinChecksum::Enhanced) == 0xE6);
    // Classic leaves the PID out
    const uint8_t aClassic[] = {0x4A, 0x55, 0x93, 0xE5};
    assert(LinCalcChecksum(0x00, aClassic, 4, LinChecksum::Classic) == 0xE6);
    assert(LinCalcChecksum(0x4A, aData, 3, LinChecksum::Classic) != 0xE6);
    // Diagnostic frames are always classic
    assert(LinCalcChecksum(LinPid(0x3C), aData, 3, LinChecksum::Enhanced) ==
           LinCalcChecksum(LinPid(0x3C), aData, 3, LinChecksum::Classic));
    // All 0xFF folds to 0xFF, inverted 0x00
    const uint8_t aFF[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    assert(LinCalcChecksum(0x00, aFF, 8, LinChecksum::Classic) == 0x00);

    // Frame timing at 19.2k
    assert(LinFrameMaxUs(1) == 3938);
    assert(LinFrameMaxUs(8) == 9042);
    assert(LinMinSlotMs(1) == 4);
    assert(LinMinSlotMs(8) == 10);

    printf("lin_check OK\n");
    return 0;
}
