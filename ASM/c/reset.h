#ifndef RESET_H
#define RESET_H

#include "z64.h"

void wait_for_reset_combo();

//Homeboy (VC) Related stuff

typedef struct
{
  unsigned int key;               /* 0x0000 */
  unsigned int sd_dram_addr;      /* 0x0004 */
  unsigned int sd_write_lba;      /* 0x0008 */
  unsigned int sd_read_lba;       /* 0x000C */
  unsigned int sd_n_blocks;       /* 0x0010 */
  unsigned int status;            /* 0x0014 */
  unsigned int dram_save_addr;    /* 0x0018 */
  unsigned int dram_save_len;     /* 0x001C */
  unsigned int dram_save_key;     /* 0x0020 */
  unsigned int timebase_hi;       /* 0x0024 */
  unsigned int timebase_lo;       /* 0x0028 */
                              /* 0x002C */
} hb_regs_t;

#define hb_regs (*(volatile hb_regs_t*)0xA8050000)
#define HB_STATUS_RESET       (0b1    << 9)
#define HB_TIMEBASE_FREQ      60750000

#endif
