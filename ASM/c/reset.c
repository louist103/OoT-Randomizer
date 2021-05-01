#include "reset.h"

static unsigned char s_reset_delay;
static uint32_t imem[] = {
    (((((uint32_t)(0x0F)) & 0x3F) << 26) | ((((uint32_t)(0)) & 0x1F) << 21) |
     ((((uint32_t)(13)) & 0x1F) << 16) |
     (((uint32_t)(0xBFC0)) & 0xFFFF)), // LUI t5, 0xBFC0
    (((((uint32_t)(0x23)) & 0x3F) << 26) | ((((uint32_t)(13)) & 0x1F) << 21) |
     ((((uint32_t)(8)) & 0x1F) << 16) |
     (((uint32_t)(0x07FC)) & 0xFFFF)), // LW T0, 0x07FC, T5
    (((((uint32_t)(0x09)) & 0x3F) << 26) | ((((uint32_t)(13)) & 0x1F) << 21) |
     ((((uint32_t)(13)) & 0x1F) << 16) |
     (((uint32_t)(0x07C0)) & 0xFFFF)), // ADDIU T5, T5, 0x07C0
    (((((uint32_t)(0x0C)) & 0x3F) << 26) | ((((uint32_t)(8)) & 0x1F) << 21) |
     ((((uint32_t)(8)) & 0x1F) << 16) |
     (((uint32_t)(0x0080)) & 0xFFFF)), // ANDI T0, T0, 0x0080
    (((((uint32_t)(0x15)) & 0x3F) << 26) | ((((uint32_t)(8)) & 0x1F) << 21) |
     ((((uint32_t)(0)) & 0x1F) << 16) |
     ((((uint32_t)(-4 * 4)) >> 2) & 0xFFFF)), // T0, R0, -4 * 4
    (((((uint32_t)(0x0F)) & 0x3F) << 26) | ((((uint32_t)(0)) & 0x1F) << 21) |
     ((((uint32_t)(13)) & 0x1F) << 16) |
     (((uint32_t)(0xBFC0)) & 0xFFFF)), // T5, 0xBFC0,
    (((((uint32_t)(0x23)) & 0x3F) << 26) | ((((uint32_t)(13)) & 0x1F) << 21) |
     ((((uint32_t)(8)) & 0x1F) << 16) |
     (((uint32_t)(0x0024)) & 0xFFFF)), // T0, 0x0024, T5
    (((((uint32_t)(0x0F)) & 0x3F) << 26) | ((((uint32_t)(0)) & 0x1F) << 21) |
     ((((uint32_t)(11)) & 0x1F) << 16) |
     (((uint32_t)(0xB000)) & 0xFFFF)), // T3, 0xB000
};

void halt_system();
void reset_system();

// Helpers for resetting the VC emulator
int hb_check() {
  if (hb_regs.key == 0x1234)
    return 0;
  else
    return -1;
}

int hb_reset()
{
  if (hb_check() == -1)
    return -1;

  hb_regs.status = HB_STATUS_RESET;
  return 0;
}

int hb_get_timebase(uint32_t *hi, uint32_t *lo)
{
  if (hb_check() == -1)
    return -1;
  if (hi)
    *hi = hb_regs.timebase_hi;
  if (lo)
    *lo = hb_regs.timebase_lo;
  return 0;
}

int hb_get_timebase64(uint64_t *tb)
{
  uint32_t hi;
  uint32_t lo;
  if (hb_get_timebase(&hi, &lo) == -1)
    return -1;
  if (tb)
    *tb = (((uint64_t)hi << 32) | (uint64_t)lo);
  return 0;
}


void wait_for_reset_combo() {
    z64_controller_t pad_pressed = z64_game.common.input[0].raw;
    
    if ((pad_pressed.pad.cr) && (pad_pressed.pad.b) && (pad_pressed.pad.s)) 
    {
        if (s_reset_delay <= 10) 
        {
            s_reset_delay++;
        } 
        else 
        {
            if (hb_check() == -1) 
            { // System is not WiiVC
                halt_system();
                /* flush data cache */
                for (uint32_t p = 0x80000000; p < 0x80002000; p += 0x10)
                    __asm__ volatile("cache   0x01, 0x0000(%[p]);" ::[p] "r"(p));

                memcpy((void *)0xA4001000, imem, sizeof(imem));
                /* copy cic boot code to rsp dmem */
                memcpy((void *)0xA4000040, (void *)0xB0000040, 0x0FC0);
                reset_system();
                __builtin_unreachable();
            } 
            else 
            { // System is WiiVC
                uint64_t tb;
                uint64_t tb_wait;
                hb_get_timebase64(&tb);
                tb_wait = tb + HB_TIMEBASE_FREQ / 2;
                while (tb < tb_wait)
                {
                    hb_get_timebase64(&tb);
                }
                hb_reset();
            }
        }
    }
    else 
    {
        s_reset_delay = 0;
    }
}

 void halt_system()
{
    /* reset cp0 status */
  __asm__ volatile ("la      $t0, 0b00110100000000000000000000000000;"
                    "mtc0    $t0, $12;" ::: "t0");
  /* halt rsp */
  __asm__ volatile ("la      $t0, 0xA4040000;"
                    "la      $t1, 0b0101010101010101010101110;"
                    "sw      $t1, 0x0010($t0);" ::: "t0", "t1");
}

 void reset_system()
{
  __asm__ volatile 
      (".set    push;"
                    ".set    noat;"
                    ".set    noreorder;"

                    /* set up parameters */
                    "lui     $t0, 0x8000;"
                    /* osRomType (0: N64, 1: 64DD) */
                    "li      $s3, 0x0000;"
                    /* osTvType (0: PAL, 1: NTSC, 2: MPAL) */
                    "lw      $s4, 0x0300($t0);"
                    /* osResetType (0: Cold, 1: NMI) */
                    "li      $s5, 0x0001;"
                    /* osCicId (3F: 6101/6102, 78: 6103, 91: 6105, 85: 6106) */
                    "li      $s6, 0x0091;"
                    /* osVersion */
                    "lw      $s7, 0x0314($t0);"
                    /* set up environment */
                    "la      $t3, 0xA4000040;"
                    "la      $sp, 0xA4001FF0;"
                    "la      $ra, 0xA4001550;"
                    /* jump to k1 */
                    "la      $t0, 0f;"
                    "la      $t1, 0x1FFFFFFF;"
                    "la      $t2, 0xA0000000;"
                    "and     $t0, $t0, $t1;"
                    "or      $t0, $t0, $t2;"
                    "jr      $t0;"
                    "nop;"
                    "0:"

                    /* decode rsp boot code */
                    "add     $t1, $sp, $zero;"
                    "lw      $t0, -0x0FF0($t1);"
                    "lw      $t2, 0x0044($t3);"
                    "xor     $t2, $t2, $t0;"
                    "sw      $t2, -0x0FF0($t1);"
                    "addi    $t3, $t3, 0x0004;"
                    "andi    $t0, $t0, 0x0FFF;"
                    "bnez    $t0, . - 0x0018;"
                    "addi    $t1, $t1, 0x0004;"
                    "lw      $t0, 0x0044($t3);"
                    "lw      $t2, 0x0048($t3);"
                    "sw      $t0, -0x0FF0($t1);"
                    "sw      $t2, -0x0FEC($t1);"
                    "sw      $zero, -0x0FE8($t1);"
                    "mtc0    $zero, $13;" /* cp0_cause */
                    "mtc0    $zero, $9;" /* cp0_count */
                    "mtc0    $zero, $11;" /* cp0_compare */

                    /* clear cache */
                    "lui     $t0, 0x8000;"
                    "addiu   $t0, $t0, 0x0000;"
                    "addiu   $t1, $t0, 0x4000;"
                    "addiu   $t1, $t1, 0xFFE0;"
                    "mtc0    $zero, $28;" /* cp0_taglo */
                    "mtc0    $zero, $29;" /* cp0_taghi */
                    "cache   0x08, 0x0000($t0);"
                    "sltu    $at, $t0, $t1;"
                    "bnez    $at, . - 0x0008;"
                    "addiu   $t0, $t0, 0x0020;"
                    "lui     $t0, 0x8000;"
                    "addiu   $t0, 0x0000;"
                    "addiu   $t1, $t0, 0x2000;"
                    "addiu   $t1, $t1, 0xFFF0;"
                    "cache   0x01, 0x0000($t0);"
                    "sltu    $at, $t0, $t1;"
                    "bnez    $at, . - 0x0008;"
                    "addiu   $t0, $t0, 0x0010;"

                    /* start rsp */
                    "addiu   $t2, $zero, 0x00CE;"
                    "lui     $at, 0xA404;"
                    "sw      $t2, 0x0010($at);"
                    "lui     $t2, 0xB000;"
                    "addiu   $t2, $t2, 0x0000;"
                    "lui     $t3, 0xFFF0;"
                    "lui     $t1, 0x0010;"
                    "and     $t2, $t2, $t3;"
                    "lui     $t0, 0xB000;"
                    "addiu   $t1, $t1, 0xFFFF;"
                    "lui     $t3, 0xB000;"
                    "addiu   $t0, $t0, 0x0554;"
                    "addiu   $t3, $t3, 0x0888;"
                    "and     $t0, $t0, $t1;"
                    "and     $t3, $t3, $t1;"
                    "lui     $at, 0xA408;"
                    "lui     $t1, 0xA000;"
                    "sw      $zero, 0x0000($at);"
                    "or      $t0, $t0, $t2;"
                    "or      $t3, $t3, $t2;"
                    "addiu   $t1, $t1, 0x0004;"
                    "lw      $t5, 0x0000($t0);"
                    "addiu   $t0, $t0, 0x0004;"
                    "sltu    $at, $t0, $t3;"
                    "addiu   $t1, $t1, 0x0004;"
                    "bnez    $at, . - 0x0010;"
                    "sw      $t5, -0x0004($t1);"
                    "addiu   $t2, $zero, 0x00AD;"
                    "lui     $at, 0xA404;"
                    "sw      $t2, 0x0010($at);"

                    /* jump to k0 */
                    "la      $t0, 0f;"
                    "la      $t1, 0x1FFFFFFF;"
                    "la      $t2, 0x80000000;"
                    "and     $t0, $t0, $t1;"
                    "or      $t0, $t0, $t2;"
                    "jr      $t0;"
                    "nop;"
                    "0:"

                    /* dma rom to ram */
                    "lui     $t3, 0xB000;"
                    "lw      $t1, 0x0008($t3);"
                    "lui     $t2, 0x1FFF;"
                    "ori     $t2, $t2, 0xFFFF;"
                    "lui     $at, 0xA460;"
                    "and     $t1, $t1, $t2;"
                    "sw      $t1, 0x0000($at);"
                    "lui     $t0, 0xA460;"
                    "lw      $t0, 0x0010($t0);"
                    "andi    $t0, $t0, 0x0002;"
                    "bnel    $t0, $zero, . - 0x0008;"
                    "lui     $t0, 0xA460;"
                    "addiu   $t0, $zero, 0x1000;"
                    "add     $t0, $t0, $t3;"
                    "and     $t0, $t0, $t2;"
                    "lui     $at, 0xA460;"
                    "sw      $t0, 0x0004($at);"
                    "lui     $t2, 0x0010;"
                    "addiu   $t2, $t2, 0xFFFF;"
                    "lui     $at, 0xA460;"
                    "sw      $t2, 0x000C($at);"
                    /* wait for dma to complete */
                    ".rept   0x10;"
                    "nop;"
                    ".endr;"
                    "lui     $t3, 0xA460;"
                    "lw      $t3, 0x0010($t3);"
                    "andi    $t3, $t3, 0x0001;"
                    "bnez    $t3, . - 0x4C;"
                    "nop;"

                    /* clear sp semaphore */
                    "lui     $at, 0xA404;"
                    "sw      $zero, 0x001C($at);"

                    /* compute checksum */
                    "lui     $t3, 0xB000;"
                    "lw      $a0, 0x0008($t3);"
                    "or      $a1, $s6, $zero;"
                    "lui     $at, 0x5D58;"
                    "ori     $at, 0x8B65;"
                    "multu   $a1, $at;"
                    "addiu   $sp, $sp, 0xFFE0;"
                    "sw      $ra, 0x001C($sp);"
                    "sw      $s0, 0x0014($sp);"
                    "lui     $s6, 0xA000;"
                    "addiu   $s6, $s6, 0x0200;"
                    "lui     $ra, 0x0010;"
                    "or      $v1, $zero, $zero;"
                    "or      $t0, $zero, $zero;"
                    "or      $t1, $a0, $zero;"
                    "mflo    $v0;"
                    "addiu   $v0, $v0, 0x0001;"
                    "or      $a3, $v0, $zero;"
                    "or      $t2, $v0, $zero;"
                    "or      $t3, $v0, $zero;"
                    "or      $s0, $v0, $zero;"
                    "or      $a2, $v0, $zero;"
                    "or      $t4, $v0, $zero;"
                    "addiu   $t5, $zero, 0x0020;"
                    "lw      $v0, 0x0000($t1);"
                    "addu    $v1, $a3, $v0;"
                    "sltu    $at, $v1, $a3;"
                    "beq     $at, $zero, . + 0x000C;"
                    "or      $a1, $v1, $zero;"
                    "addiu   $t2, $t2, 0x0001;"
                    "andi    $v1, $v0, 0x001F;"
                    "subu    $t7, $t5, $v1;"
                    "srlv    $t8, $v0, $t7;"
                    "sllv    $t6, $v0, $v1;"
                    "or      $a0, $t6, $t8;"
                    "sltu    $at, $a2, $v0;"
                    "or      $a3, $a1, $zero;"
                    "xor     $t3, $t3, $v0;"
                    "beq     $at, $zero, . + 0x0014;"
                    "addu    $s0, $s0, $a0;"
                    "xor     $t9, $a3, $v0;"
                    "beq     $zero, $zero, . + 0x000C;"
                    "xor     $a2, $t9, $a2;"
                    "xor     $a2, $a2, $a0;"
                    "lw      $t7, 0x0000($s6);"
                    "addiu   $t0, $t0, 0x0004;"
                    "addiu   $s6, $s6, 0x0004;"
                    "xor     $t7, $v0, $t7;"
                    "addu    $t4, $t7, $t4;"
                    "lui     $t7, 0xA000;"
                    "ori     $t7, $t7, 0x02FF;"
                    "addiu   $t1, $t1, 0x0004;"
                    "bne     $t0, $ra, . - 0x0070;"
                    "and     $s6, $s6, $t7;"
                    "xor     $t6, $a3, $t2;"
                    "xor     $a3, $t6, $t3;"

                    /* halt rsp */
                    "lui     $t3, 0x00AA;"
                    "ori     $t3, 0xAAAE;"
                    "lui     $at, 0xA404;"
                    "sw      $t3, 0x0010($at);"

                    /* set mi intr mask (clear all) */
                    "lui     $at, 0xA430;"
                    "addiu   $t0, $zero, 0x0555;"
                    "sw      $t0, 0x000C($at);"
                    /* clear si interrupt */
                    "lui     $at, 0xA480;"
                    "sw      $zero, 0x0018($at);"
                    /* clear ai  interrupt */
                    "lui     $at, 0xA450;"
                    "sw      $zero, 0x000C($at);"
                    /* clear dp interrupt */
                    "lui     $at, 0xA430;"
                    "addiu   $t1, $zero, 0x0800;"
                    "sw      $t1, 0x0000($at);"
                    /* clear pi interrupt */
                    "addiu   $t1, $zero, 0x0002;"
                    "lui     $at, 0xA460;"
                    "sw      $t1, 0x0010($at);"

                    /* save startup info */
                    "lui     $t0, 0xA000;"
                    "ori     $t0, $t0, 0x0300;"
                    "xor     $t8, $s0, $a2;" /* checksum code */
                    "addiu   $t1, $zero, 0x17D9;"
                    "xor     $s0, $t8, $t4;" /* checksum code */
                    "sw      $t1, 0x0010($t0);"
                    "sw      $s4, 0x0000($t0);"
                    "sw      $s3, 0x0004($t0);"
                    "sw      $s5, 0x000C($t0);"
                    "beq     $s3, $zero, . + 0x0014;" /* check reset type */
                    "sw      $s7, 0x0014($t0);"
                    "lui     $t1, 0xA600;"
                    "beq     $zero, $zero, . + 0x0010;"
                    "addiu   $t1, 0x0000;"
                    "lui     $t1, 0xB000;"
                    "addiu   $t1, $t1, 0x0000;"
                    "sw      $t1, 0x0008($t0);"
                    "lw      $t1, 0x00F0($t0);"
                    "lui     $t3, 0xB000;"
                    "sw      $t1, 0x0018($t0);"

                    /* check checksum */
                    "lw      $t0, 0x0010($t3);"
                    "bne     $a3, $t0, . + 0x001C;"
                    "nop;"
                    "lw      $t0, 0x0014($t3);"
                    "bne     $s0, $t0, . + 0x0010;"
                    "nop;"
                    "bgezal  $zero, . + 0x0010;"
                    "nop;"
                    "bgezal  $zero, .;"
                    "nop;"

                    /* clear sp mem */
                    "lui     $t0, 0xA400;"
                    "addiu   $t0, $t0, 0x0000;"
                    "lw      $s0, 0x0014($sp);" /* checksum code */
                    "lw      $ra, 0x001C($sp);" /* checksum code */
                    "addiu   $sp, $sp, 0x0020;" /* checksum code */
                    "addi    $t1, $t0, 0x2000;"
                    "addiu   $t0, $t0, 0x0004;"
                    "bne     $t0, $t1, . - 0x0004;"
                    "sw      $t1, -0x0004($t0);"

                    /* jump to game preamble */
                    "lui     $t0, 0xB000;"
                    "lw      $t0, 0x0008($t0);"
                    "la      $t1, 0x80400000;"
                    "lw      $t2, 0x0000($t0);"
                    "la      $t3, 0x3C02A805;"
                    "beq     $t2, $t3, . + 0x0010;"
                    "nop;"
                    "jr      $t0;"
                    "nop;"
                    "jr      $t1;"
                    "nop;"
                    ".set    pop;");
}