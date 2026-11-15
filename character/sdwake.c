/*
 * sdwake.c -- hook U's second half (2.3.11-mod, GitHub issue #6): the cause.
 *
 * Measured 2026-11-07 on a Mac, with no module involved: the owner's 8 GB
 * Longsys card (LSMSSD0) answers the first read after 20-30 s idle in 5.5 to
 * 8.35 s, and in 4-6 ms otherwise; one small read every 10-18 s keeps it fast.
 * A 64 GB SanDisk measured beside it never slept (2-3 ms after 90 s). On the
 * module (2.3.11-sd4's recorder, twice): about 22 s with no card access, then
 * a one-sector read of a ready card got no data and no error in stock's 5 s.
 * Stock then fails the read without stopping it, so the card is left sending:
 * every read after it fails, the card check restarts the powered card, and
 * the card can stay busy until it loses power (stock: a boot loop).
 *   - keep-alive: once the card has done nothing for SDK_IDLE_MS, the file
 *     thread reads again, at its idle point, the sector FatFs's window holds,
 *     into that window: the same bytes, so FatFs sees nothing, and the card
 *     never falls asleep (no window to spare: patch RAM has no 512 bytes left);
 *   - patience: SD_read's and SD_write's wait for the transfer is 9 s, not 5
 *     (patch_micro.py SDK_IMMEDIATES), still under the watchdog's 10 s
 *     because the ready poll just before it stamped the heartbeat;
 *   - a transfer still outstanding when the wait gives up is stopped (CMD12,
 *     the data path cleared, the handle READY), and once the card is back in
 *     TRANSFER the read or write is tried once more, before FatFs sees a failure.
 * The heartbeat is stamped only on the card's answers, as hook U does.
 */
#include <stdint.h>

#define SDK_RAM          0x2406c020u  /* patch_micro.py SDK_RAM: sdk_t */
#define SDK_FATFS        0x2400a4f0u  /* FatFs[0], the volume (find_volume 0x08055e3c) */
#define FS_TYPE          0x00u        /* the volume: 0 not mounted */
#define FS_DRV           0x01u
#define FS_WFLAG         0x03u        /* the window holds unsaved changes */
#define FS_WINSECT       0x38u        /* the sector in the window, -1 none */
#define FS_WIN           0x3cu        /* the window, 512 bytes in the uncached pool */
#define SDK_IDLE_MS      10000u       /* the card's sleep began between 20 and 30 s */
#define SDK_SETTLE_MS    2000u        /* after a stop, for the card to come back to TRANSFER */
#define SDK_ABORT_MS     50u          /* for the data path to report the stop */
#define SDK_TICK         0x24013848u  /* FreeRTOS xTickCount, ms */
#define SDK_HSD          0x24003044u  /* the SD handle: +0x2c Context, +0x30 State, +0x34 ErrorCode */
#define SDK_SDMMC        0x52007000u  /* SDMMC1 */
#define SDK_CARD_OK      0x24009c95u  /* the card check's verdict (0x0804441e): 1 = mounted */
#define SDK_HB           0x24009cacu  /* the heartbeats: +0 the file thread, +8 the block count */
#define SDK_DISKIO_READ  0x080bf038u  /* diskio's read entry, whatever wraps it */
#define SDK_MAGIC        0x53444b31u  /* "SDK1" */

#define HSD_READY        1u
#define HSD_BUSY         3u
#define CARD_TRANSFER    4u

#define REG(a)  (*(volatile uint32_t *)(uintptr_t)(a))
#define REG8(a) (*(volatile uint8_t *)(uintptr_t)(a))

typedef struct {
    uint32_t magic;
    uint32_t last;                   /* the tick of the card's last read or write */
    uint32_t ka, kafail;             /* keep-alive reads, and those that failed */
    uint32_t tries, ok;              /* stuck transfers stopped and tried again, and those that then worked */
} sdk_t;

extern void sdk_x_stamp(void *hb);                                                        /* 0x08044f90 */
extern uint32_t sdk_x_read(uint32_t lun, uint8_t *buf, uint32_t sector, uint32_t count);  /* SD_read */
extern uint32_t sdk_x_write(uint32_t lun, const uint8_t *buf, uint32_t sector, uint32_t count);   /* SD_write */
extern uint32_t sdk_x_cmd12(uint32_t sdmmc);                     /* SDMMC_CmdStopTransfer, 0x0804c9e4 */
extern uint32_t sdk_x_state(void *hsd);                          /* HAL_SD_GetCardState, 0 with no answer */

static volatile sdk_t *sdk(void)
{
    volatile sdk_t *s = (volatile sdk_t *)(uintptr_t)SDK_RAM;
    if (s->magic != SDK_MAGIC) {     /* power-on: patch RAM holds noise */
        s->last = REG(SDK_TICK);
        s->ka = s->kafail = s->tries = s->ok = 0u;
        s->magic = SDK_MAGIC;
    }
    return s;
}

/* the transfer SD_read or SD_write gave up on is still running */
static int sdk_stuck(void)
{
    return REG8(SDK_HSD + 0x30u) == HSD_BUSY;
}

/* stop it, as HAL_SD_Abort would: the transfer's interrupts off, the stop
 * command, the data path and IDMA off, the flags cleared, the handle READY */
static void sdk_stop(void)
{
    REG(SDK_SDMMC + 0x3cu) &= ~0x13au;   /* MASK: DCRCFAIL, DTIMEOUT, TXUNDERR, RXOVERR, DATAEND */
    REG(SDK_SDMMC + 0x0cu) &= ~0x40u;    /* CMDR: CMDTRANS */
    (void)sdk_x_cmd12(SDK_SDMMC);
    uint32_t t0 = REG(SDK_TICK);
    while (!(REG(SDK_SDMMC + 0x34u) & 0x900u) && REG(SDK_TICK) - t0 < SDK_ABORT_MS) { }   /* DABORT or DATAEND */
    REG(SDK_SDMMC + 0x50u) = 0u;         /* IDMACTRL */
    REG(SDK_SDMMC + 0x2cu) = 0u;         /* DCTRL */
    REG(SDK_SDMMC + 0x38u) = 0x1fe00fffu;    /* ICR: the static flags, as stock's DMA start clears them */
    REG(SDK_HSD + 0x34u) = 0u;
    REG(SDK_HSD + 0x2cu) = 0u;
    REG8(SDK_HSD + 0x30u) = HSD_READY;
}

/* until the card is back in TRANSFER; each answer stamps the heartbeat */
static int sdk_settle(void)
{
    uint32_t t0 = REG(SDK_TICK);
    do {
        uint32_t st = sdk_x_state((void *)(uintptr_t)SDK_HSD);
        if (st) REG(SDK_HB) = REG(SDK_HB + 8u);
        if (st == CARD_TRANSFER) { REG(SDK_HSD + 0x34u) = 0u; return 1; }
    } while (REG(SDK_TICK) - t0 < SDK_SETTLE_MS);
    REG(SDK_HSD + 0x34u) = 0u;
    return 0;
}

uint32_t sdk_rd(uint32_t lun, uint8_t *buf, uint32_t sector, uint32_t count)
{
    volatile sdk_t *s = sdk();
    uint32_t r = sdk_x_read(lun, buf, sector, count);
    if (r && sdk_stuck()) {
        s->tries = s->tries + 1u;
        sdk_stop();
        if (sdk_settle()) {
            r = sdk_x_read(lun, buf, sector, count);
            if (!r) s->ok = s->ok + 1u;
        }
    }
    s->last = REG(SDK_TICK);
    return r;
}

uint32_t sdk_wr(uint32_t lun, const uint8_t *buf, uint32_t sector, uint32_t count)
{
    volatile sdk_t *s = sdk();
    uint32_t r = sdk_x_write(lun, buf, sector, count);
    if (r && sdk_stuck()) {
        s->tries = s->tries + 1u;
        sdk_stop();
        if (sdk_settle()) {
            r = sdk_x_write(lun, buf, sector, count);
            if (!r) s->ok = s->ok + 1u;
        }
    }
    s->last = REG(SDK_TICK);
    return r;
}

/* the file thread's idle point (0x0809ab30: no stream message waiting, so
 * FatFs is between calls), in place of its heartbeat stamp: the stamp, then a
 * keep-alive read once the card has been idle SDK_IDLE_MS, through diskio's
 * entry (so whatever wraps it sees it), only with the card mounted, no
 * transfer running and nothing unsaved in the window */
void sdk_idle(void *hb)
{
    sdk_x_stamp(hb);
    volatile sdk_t *s = sdk();
    uint32_t now = REG(SDK_TICK);
    if (now - s->last < SDK_IDLE_MS) return;
    s->last = now;
    if (REG8(SDK_CARD_OK) != 1u || REG8(SDK_HSD + 0x30u) != HSD_READY) return;
    uint32_t fs = REG(SDK_FATFS);
    if (fs < 0x24000000u || fs >= 0x24080000u || !REG8(fs + FS_TYPE) || REG8(fs + FS_WFLAG)) return;
    uint32_t sect = REG(fs + FS_WINSECT);
    uint32_t (*rd)(uint32_t, uint8_t *, uint32_t, uint32_t) =
        (uint32_t (*)(uint32_t, uint8_t *, uint32_t, uint32_t))(uintptr_t)REG(SDK_DISKIO_READ);
    s->ka = s->ka + 1u;
    if (rd(REG8(fs + FS_DRV), (uint8_t *)(uintptr_t)(fs + FS_WIN), sect == 0xffffffffu ? 0u : sect, 1u)) {
        s->kafail = s->kafail + 1u;
        REG(fs + FS_WINSECT) = 0xffffffffu;   /* what the window holds is unknown now: FatFs reads it again */
    }
}
