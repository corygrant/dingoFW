MEMORY
{
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 8K /* App starts at 0x08002000 */
  RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 12K
}

INCLUDE sections.ld
