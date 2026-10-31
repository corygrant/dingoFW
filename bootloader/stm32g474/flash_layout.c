// Included by OpenBLT flash.c. First 16KB is the bootloader, the last 8KB is config (CONFIG_SECTOR 252-255)
// Dual bank (factory default, 2KB pages): no entry may cross the bank 2 start at 0x08040000
static const tFlashSector flashLayout[] =
{
  { 0x08004000, 0x04000 },
  { 0x08008000, 0x08000 },
  { 0x08010000, 0x10000 },
  { 0x08020000, 0x20000 },
  { 0x08040000, 0x20000 },
  { 0x08060000, 0x10000 },
  { 0x08070000, 0x08000 },
  { 0x08078000, 0x06000 },
};
