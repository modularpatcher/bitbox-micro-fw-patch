/*
 * bootdump.c -- the chip report (patch_micro.py --v239 --boot-dump): a
 * diagnostic build on 2.3.9-mod, never a release. It stays under 768 KB, so
 * it flashes on every module.
 *
 * Why (GitHub issue, 2026-11-16): on some modules every release from
 * 2.3.10-mod fails with the bootloader's "Unable to verify", while 2.3.9-mod
 * flashes. Every failing image runs past 0x08100000, where bank 2 of a 2 MB
 * STM32H743 starts; on the owner's module they all flash. The owner's
 * bootloader (bd1's dump) erases bank 2 and programs up to 0x081DFFE0; the
 * theory is that older ones stop at bank 1, so only the verify notices.
 *
 * bd2 writes one text file, REPORT.TXT, for a user to paste into the issue.
 * It names no serial number (not the chip's unique id, not the card's) and
 * carries none of 1010music's code: the bootloader is summarised on the
 * module -- its CRC-32 (zlib's), how much of it is used, and every word in it
 * that is an address in the application's flash, which is where its
 * programming limit shows (the owner's: 081dffe0), and (bd3) a CRC-32 for
 * each 4 KB block, so a bootloader that differs from the owner's shows where,
 * even if its limit is not a plain address -- and so is bank 2, a
 * line a sector: protected, blank, or its CRC-32 and first two words. Then
 * the chip (device id, revision, flash size, the option registers) and the
 * card (type, size, the CID's maker, OEM, product, revision and date; not
 * its serial).
 *
 * It runs at the file thread's first tml read after power-on (settings.tml,
 * 0x0808edea: f_open(fil, path, FA_READ)), once per card: REPORT.TXT is
 * created first, so a fault restarts once (the watchdog) and the next boot
 * finds it and runs as 2.3.9-mod. No read is made inside a protected or
 * secure-only area (a bus error there), nor of bank 2 on a chip that does
 * not report 2 MB.
 *
 * Each f_write is one line, under a sector: FatFs (FF_FS_TINY) copies it into
 * its window. The file thread's heartbeat is stamped every 4 KB read, as
 * stock's stamp (0x08044f90) does: heartbeats[0] = heartbeats[2].
 *
 * Rules (the C object's, see patch_micro.py): no .data/.bss, no libc.
 */
#include <stdint.h>

extern uint32_t bd_x_fopen(void *fil, const char *path, uint32_t mode);                  /* 0x08056edc */
extern uint32_t bd_x_fwrite(void *fil, const void *buf, uint32_t n, uint32_t *written);  /* 0x0805732c */
extern uint32_t bd_x_fclose(void *fil);                                                  /* 0x080576a6 */

#define BD_BUILD         "2.3.9-dg1"   /* the owner's name for it; bd1-bd3 while it was built */
#define BD_FIL_BYTES     0x58u         /* the firmware's FIL, as the tml read clears it (0x0808edda) */
#define BD_HB            0x24009cacu   /* the heartbeats: [0] the file thread's, [2] the block count */
#define BD_FA_READ       0x01u
#define BD_FA_NEW        0x0au         /* FA_WRITE | FA_CREATE_ALWAYS */
#define BD_IDCODE        0x5c001000u   /* DBGMCU_IDCODE: REV_ID << 16 | DEV_ID */
#define BD_FLASHSIZE     0x1ff1e880u   /* u16, KB (H742/H743/H753) */
#define BD_FLASH         0x52002000u
#define BD_HSD           0x24003044u   /* the SD handle (SD_InitCard, 0x0804d3e0-0x0804d452) */
#define BD_BOOT          0x08000000u
#define BD_BOOT_BYTES    0x40000u
#define BD_APP           0x08040000u
#define BD_FLASH_END     0x08200000u
#define BD_BANK2         0x08100000u
#define BD_SECTOR        0x20000u
#define BD_REFS          40u

static uint32_t bd_reg(uint32_t a) { return *(volatile const uint32_t *)(uintptr_t)a; }

static void bd_stamp(void)
{
    volatile uint32_t *hb = (volatile uint32_t *)(uintptr_t)BD_HB;
    hb[0] = hb[2];
}

static void bd_clear(uint8_t *fil)
{
    for (uint32_t k = 0; k < BD_FIL_BYTES; k++) ((volatile uint8_t *)fil)[k] = 0u;
}

/* a protected (PRAR) or secure-only (SCAR) area register: start in bits 11:0,
 * end in 27:16, in 256-byte units from the bank's start; start > end is none */
static int bd_covers(uint32_t reg, uint32_t first, uint32_t last)
{
    uint32_t s = reg & 0xfffu, e = (reg >> 16) & 0xfffu;
    return s <= e && s <= last && e >= first;
}

/* zlib's CRC-32, a bit at a time: no table, no .data */
static uint32_t bd_crc(uint32_t from, uint32_t n)
{
    uint32_t c = 0xffffffffu;
    for (uint32_t i = 0; i < n; i++) {
        c ^= *(volatile const uint8_t *)(uintptr_t)(from + i);
        for (int b = 0; b < 8; b++) c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1u)));
        if (!(i & 0xfffu)) bd_stamp();
    }
    return ~c;
}

/* ---- a line of text, then f_write ---------------------------------------- */

typedef struct { char s[124]; uint32_t n; } bd_line_t;

static void bd_put(bd_line_t *l, const char *t)
{
    while (*t && l->n < sizeof l->s - 2u) l->s[l->n++] = *t++;
}

static void bd_chr(bd_line_t *l, char c)
{
    if (l->n < sizeof l->s - 2u) l->s[l->n++] = c;
}

static void bd_hex(bd_line_t *l, uint32_t v, int digits)
{
    for (int k = digits - 1; k >= 0; k--) bd_chr(l, "0123456789abcdef"[(v >> (4 * k)) & 15u]);
}

static void bd_dec(bd_line_t *l, uint32_t v)
{
    char d[10]; int n = 0;
    do { d[n++] = (char)('0' + v % 10u); v /= 10u; } while (v && n < 10);
    while (n) bd_chr(l, d[--n]);
}

/* printable ASCII only: a CID field may hold anything */
static void bd_ascii(bd_line_t *l, uint32_t c)
{
    bd_chr(l, c >= 0x20u && c < 0x7fu ? (char)c : '?');
}

static uint32_t bd_emit(uint8_t *fil, bd_line_t *l)
{
    uint32_t done = 0;
    l->s[l->n++] = '\r';
    l->s[l->n++] = '\n';
    uint32_t r = bd_x_fwrite(fil, l->s, l->n, &done) || done != l->n;
    l->n = 0;
    return r;
}

/* ---- the report ----------------------------------------------------------- */

static uint32_t bd_report(uint8_t *fil)
{
    bd_line_t l;
    uint32_t bad = 0;
    l.n = 0;

    uint32_t id = bd_reg(BD_IDCODE), kb = *(volatile const uint16_t *)(uintptr_t)BD_FLASHSIZE;
    uint32_t prar1 = bd_reg(BD_FLASH + 0x028u), scar1 = bd_reg(BD_FLASH + 0x030u);
    uint32_t prar2 = bd_reg(BD_FLASH + 0x128u), scar2 = bd_reg(BD_FLASH + 0x130u);

    bd_put(&l, "Bitbox Micro chip report " BD_BUILD); bad |= bd_emit(fil, &l);
    bd_put(&l, "Paste all of this into the GitHub issue. It holds no serial numbers."); bad |= bd_emit(fil, &l);
    bad |= bd_emit(fil, &l);

    bd_put(&l, "chip        STM32H7 0x"); bd_hex(&l, id & 0xfffu, 3);
    bd_put(&l, ", revision 0x"); bd_hex(&l, id >> 16, 4);
    bd_put(&l, ", flash "); bd_dec(&l, kb); bd_put(&l, " KB"); bad |= bd_emit(fil, &l);
    bd_put(&l, "options     OPTSR "); bd_hex(&l, bd_reg(BD_FLASH + 0x01cu), 8);
    bd_put(&l, "  BOOT "); bd_hex(&l, bd_reg(BD_FLASH + 0x040u), 8); bad |= bd_emit(fil, &l);
    bd_put(&l, "protection  bank 1: PRAR "); bd_hex(&l, prar1, 8);
    bd_put(&l, "  SCAR "); bd_hex(&l, scar1, 8);
    bd_put(&l, "  WPSN "); bd_hex(&l, bd_reg(BD_FLASH + 0x038u), 8); bad |= bd_emit(fil, &l);
    bd_put(&l, "            bank 2: PRAR "); bd_hex(&l, prar2, 8);
    bd_put(&l, "  SCAR "); bd_hex(&l, scar2, 8);
    bd_put(&l, "  WPSN "); bd_hex(&l, bd_reg(BD_FLASH + 0x138u), 8); bad |= bd_emit(fil, &l);

    /* the bootloader, summarised */
    uint32_t last = (BD_BOOT_BYTES >> 8) - 1u;
    if (bd_covers(prar1, 0u, last) || bd_covers(scar1, 0u, last)) {
        bd_put(&l, "bootloader  protected, not read"); bad |= bd_emit(fil, &l);
    } else {
        uint32_t used = BD_BOOT_BYTES;
        while (used && *(volatile const uint8_t *)(uintptr_t)(BD_BOOT + used - 1u) == 0xffu) used--;
        bd_put(&l, "bootloader  crc "); bd_hex(&l, bd_crc(BD_BOOT, BD_BOOT_BYTES), 8);
        bd_put(&l, ", "); bd_dec(&l, used); bd_put(&l, " bytes used"); bad |= bd_emit(fil, &l);
        uint32_t refs[BD_REFS], n = 0;
        for (uint32_t a = BD_BOOT; a + 4u <= BD_BOOT + used; a += 4u) {
            uint32_t v = bd_reg(a);
            if (v < BD_APP || v >= BD_FLASH_END) continue;
            uint32_t k = 0;
            while (k < n && refs[k] < v) k++;
            if (k < n && refs[k] == v) continue;
            if (n == BD_REFS) continue;                    /* full: the rest go unlisted */
            for (uint32_t j = n; j > k; j--) refs[j] = refs[j - 1u];
            refs[k] = v;
            n++;
        }
        bd_put(&l, "            refers to");
        for (uint32_t k = 0; k < n; k++) {
            if (k && !(k % 7u)) { bad |= bd_emit(fil, &l); bd_put(&l, "           "); }
            bd_chr(&l, ' '); bd_hex(&l, refs[k], 8);
        }
        if (!n) bd_put(&l, " nothing");
        bad |= bd_emit(fil, &l);
        for (uint32_t b = 0; b < BD_BOOT_BYTES; b += 0x1000u) {     /* bd3: the block map */
            if (!(b & 0x7fffu)) {
                if (b) bad |= bd_emit(fil, &l);
                bd_put(&l, b ? "                " : "            map "); bd_hex(&l, b, 5);
            }
            bd_chr(&l, ' '); bd_hex(&l, bd_crc(BD_BOOT + b, 0x1000u), 8);
        }
        bad |= bd_emit(fil, &l);
    }

    /* bank 2, a sector a line */
    if (kb < 2048u || kb == 0xffffu) {
        bd_put(&l, "bank 2      not read: the chip reports under 2 MB"); bad |= bd_emit(fil, &l);
    } else {
        for (uint32_t s = 0; s < 8u; s++) {
            uint32_t a = BD_BANK2 + s * BD_SECTOR, first = s * (BD_SECTOR >> 8), lastu = first + (BD_SECTOR >> 8) - 1u;
            bd_put(&l, s ? "            sector " : "bank 2      sector ");
            bd_dec(&l, s); bd_put(&l, "  "); bd_hex(&l, a, 8); bd_put(&l, "  ");
            if (bd_covers(prar2, first, lastu) || bd_covers(scar2, first, lastu)) {
                bd_put(&l, "protected, not read");
            } else {
                uint32_t blank = 1;
                for (uint32_t k = 0; k < BD_SECTOR && blank; k += 4u) {
                    if (bd_reg(a + k) != 0xffffffffu) blank = 0;
                    if (!(k & 0xfffu)) bd_stamp();
                }
                if (blank) {
                    bd_put(&l, "blank");
                } else {
                    bd_put(&l, "used  crc "); bd_hex(&l, bd_crc(a, BD_SECTOR), 8);
                    bd_put(&l, "  starts "); bd_hex(&l, bd_reg(a), 8);
                    bd_chr(&l, ' '); bd_hex(&l, bd_reg(a + 4u), 8);
                }
            }
            bad |= bd_emit(fil, &l);
        }
    }

    /* the card: SD_InitCard's CardInfo (+0x38), CID (+0x6c); never its serial */
    uint32_t type = bd_reg(BD_HSD + 0x38u), ver = bd_reg(BD_HSD + 0x3cu), ccc = bd_reg(BD_HSD + 0x40u);
    uint32_t blocks = bd_reg(BD_HSD + 0x50u), bsize = bd_reg(BD_HSD + 0x54u);
    uint32_t c0 = bd_reg(BD_HSD + 0x6cu), c1 = bd_reg(BD_HSD + 0x70u), c2 = bd_reg(BD_HSD + 0x74u), c3 = bd_reg(BD_HSD + 0x78u);
    uint32_t date = (c3 >> 8) & 0xfffu;
    bd_put(&l, "card        type "); bd_dec(&l, type); bd_put(&l, " v"); bd_dec(&l, ver);
    bd_put(&l, ", "); bd_dec(&l, blocks); bd_put(&l, " blocks of "); bd_dec(&l, bsize);
    bd_put(&l, ", classes "); bd_hex(&l, ccc, 3); bad |= bd_emit(fil, &l);
    bd_put(&l, "            maker "); bd_hex(&l, c0 >> 24, 2);
    bd_chr(&l, ' '); bd_ascii(&l, (c0 >> 16) & 0xffu); bd_ascii(&l, (c0 >> 8) & 0xffu);
    bd_chr(&l, ' '); bd_ascii(&l, c0 & 0xffu);
    for (int k = 3; k >= 0; k--) bd_ascii(&l, (c1 >> (8 * k)) & 0xffu);
    bd_put(&l, " rev "); bd_dec(&l, (c2 >> 28) & 15u); bd_chr(&l, '.'); bd_dec(&l, (c2 >> 24) & 15u);
    bd_put(&l, ", made "); bd_dec(&l, 2000u + (date >> 4)); bd_chr(&l, '-'); bd_dec(&l, date & 15u);
    bad |= bd_emit(fil, &l);
    return bad;
}

/* the tml read's f_open, taken: the report once per card, then stock's call */
uint32_t bd_fopen(uint8_t *fil, const char *path, uint32_t mode)
{
    bd_clear(fil);
    if (bd_x_fopen(fil, "REPORT.TXT", BD_FA_READ) == 0u) {
        bd_x_fclose(fil);
    } else {
        bd_clear(fil);
        if (bd_x_fopen(fil, "REPORT.TXT", BD_FA_NEW) == 0u) {
            bd_report(fil);
            bd_x_fclose(fil);
        }
    }
    bd_clear(fil);
    return bd_x_fopen(fil, path, mode);
}
