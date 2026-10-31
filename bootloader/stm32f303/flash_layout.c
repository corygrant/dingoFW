// Included by OpenBLT flash.c. First 8KB is the bootloader, the last 2KB page is config (CONFIG_SECTOR 31)
static const tFlashSector flashLayout[] =
{
  { 0x08002000, 0x2000 },
  { 0x08004000, 0x4000 },
  { 0x08008000, 0x7800 },
};
