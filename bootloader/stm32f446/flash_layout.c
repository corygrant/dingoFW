// Included by OpenBLT flash.c. Sector 0 is the bootloader, the app gets the rest (config lives in FRAM)
static const tFlashSector flashLayout[] =
{
  { 0x08004000, 0x04000, 1 },
  { 0x08008000, 0x04000, 2 },
  { 0x0800C000, 0x04000, 3 },
  { 0x08010000, 0x10000, 4 },
  { 0x08020000, 0x20000, 5 },
  { 0x08040000, 0x20000, 6 },
  { 0x08060000, 0x20000, 7 },
};
