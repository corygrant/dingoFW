MEMORY
{
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 16K /* App starts at 0x08004000 */
  RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 32K /* Clear of the handoff block at 0x2001FFF0 */
}

INCLUDE sections.ld
