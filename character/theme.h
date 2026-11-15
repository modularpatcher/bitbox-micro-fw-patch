/*
 * theme.h -- 2.3.10-b20's screen themes (CHR_CMP builds only).
 *
 * Every colour on 1010's screens comes from one palette of 34 ARGB8888
 * entries in RAM at 0x24000000, copied there at boot from flash 0x080e3d98
 * (the .data image) and never rewritten by stock afterwards. Its fill
 * primitive (0x08042980) reads palette[index] each time it draws
 * (0x080425d4), and indexed images convert through the DMA2D's hardware CLUT,
 * which boot loads from the same table (0x080425e0). So a theme is 34 words:
 * written into the table, the CLUT reloaded the way boot loads it, and the
 * UI's widget tree marked dirty, which makes the renderer (0x0805bafc) clear
 * its canvas and repaint every visible widget (0x08060be6 skips none).
 *
 * The Settings row (TOOLS > Settings, page 3) is an enum like Keep CCs': id
 * THM_ID, saved in settings.tml as `theme`. Hook T hands its value to
 * thm_post at boot and on every edit; the UI tick (chr_osd_tick) applies it.
 *
 * The themes recolour the palette family by family (THEMES.md has the
 * design): the pink selection, the cyan accent, the grey cells, the
 * backgrounds, the status colours. (b20-b25 test builds also had Probe 1-6,
 * each painting six entries loud for photos: THEMES.md has their results;
 * removed in b26, so a saved probe value is out of range: Stock.)
 *
 * b23, Custom: the 20 colours of theme.tml at the top of the card, the file
 * the theme maker writes (`<document><theme background="#141414" .../>`), by
 * the same names as THEMES.md's families. It is read on the file thread
 * (0x0808e568's job 4, the settings load: hook T runs thm_job first) with
 * stock's own loader and TinyXML2, as settings.tml is; a colour missing or
 * mistyped is Stock's. Custom chosen with no theme.tml takes the colours on
 * screen and writes them out at the next settings save (job 5), so there is
 * always a working file to edit.
 *
 * b37, the owner: no restart for a new file. Each time the row is set to
 * Custom (from another theme), the file is read again at that edit's settings
 * save, so a card swapped or a file edited shows at once. Not there then (or
 * the card unreadable), the colours on screen stay and nothing is written.
 * b38: b37's re-read found no file on a card swapped while running (the
 * owner, with b38's pop-up): stock remounts only when its own card check
 * fails, which its preset jobs run first. The re-read now runs it too
 * (hardware-confirmed on b39: a swapped card's theme shows, no restart).
 *
 * THM_SAFE (2.3.11, GitHub issue #6): b38's check ran at EVERY re-read, at the
 * head of a settings save, often milliseconds after the save before it. On a
 * budget card (an 8 GB Longsys SDHC, 2026-11-03) that froze the file thread in
 * the card's power-up (ACMD41) loop -- FatFs re-initialising a card still busy
 * with the last write -- and the watchdog then boot-looped the module until
 * the card lost power. Stock never probes a card that soon after a write. So:
 *   - the re-read opens theme.tml first and runs the check only when that
 *     fails (the swapped card b38 was for), then tries once more;
 *   - it never runs within THM_SETTLE of the last settings save: the file
 *     thread notes when each save starts, a re-read due sooner waits, and the
 *     UI tick posts one more save once the card has settled;
 *   - it runs only while the row is still on Custom, and never on boot's own
 *     post of the saved value (a restart that kept patch RAM could otherwise
 *     arm it with no edit at all).
 * THM_HOTSWAP (with THM_SAFE; owner's bench 2026-11-03): THM_SAFE's open-first
 * missed a card edited on a computer and put back while running -- the open
 * succeeds against FatFs's stale view and reads the old colours. So the card
 * check runs before every re-read again, as b38 did, but only ever THM_SETTLE
 * after the last save (thm_job), when even a slow card is done writing. It
 * restores the instant update, the new colours a moment after the row reaches
 * Custom. dg8 (2026-11-04) still froze on the budget card: the check's
 * remount ran into 1010's card start-up, which waits up to 35 s. 2.3.11-mod's
 * hook U (patch_micro.py) bounds that start-up, so 2.3.11 keeps the check.
 * THM_NOW (2.3.11, with THM_SAFE and THM_HOTSWAP; owner's bench 2026-11-04):
 * the TOOLS top bar (the version, at 200,0) kept the theme BEFORE the one
 * chosen. An edit repaints it, but that repaint came before the UI tick put
 * the new palette in, and the tick's repaint of the tree does not redraw it.
 * So thm_post, on the UI thread at the edit, puts the palette in at once
 * (thm_apply), before the edit's own repaint; the tick stays the backstop
 * (boot, and Custom's colours read later from theme.tml).
 */
#ifndef THEME_H
#define THEME_H

#define THM_ID           0x183u       /* patch_micro.py: THEME_ID (unregistered in stock, never an immediate) */
#define THM_RAM          0x2406a644u  /* patch RAM: thm_ram_t (patch_micro.py: THEME_RAM) */
#define THM_MAGIC        0x54484d45u  /* "THME" */
#define THM_PAL          0x24000000u  /* the palette, in RAM */
#define THM_PAL_FLASH    0x080e3d98u  /* ... and as boot copies it, from flash: Stock */
#define THM_N            34u
#define THM_BUILT        6u           /* Stock, then the five */
#define THM_CUSTOM       6u           /* b23: theme.tml */
#define THM_COUNT        7u           /* Stock, the five, Custom */
#define THM_FAM          20u          /* theme.tml's colours */
#define THM_CMAGIC       0x54484d43u  /* "THMC": col[] and flags are set */
#define THM_F_FILE       1u           /* theme.tml was on the card at the last settings load */
#define THM_F_WRITE      2u           /* write the starter file at the next settings save */
#define THM_F_RELOAD     4u           /* b37: read theme.tml again at the next settings save */
#ifdef THM_SAFE
#define THM_F_BOOT       8u           /* boot's settings load ran: the next thm_post is boot's own */
#define THM_SETTLE       600000000u   /* 1.5 s at ~400 MHz (osd_now): the card done with the last save */
#endif
#define THM_SDCARD       0x24009c80u  /* the SD card's object, as the preset jobs pass it (patch_micro.py) */
#define THM_FIL_PTR      0x24022970u  /* the file thread's FIL (0x0808edc4, 0x0808ed14) */
#define THM_FIL_BYTES    0x58u
#define THM_DOC_BYTES    0x170u       /* an XMLDocument, as 0x0805bdd8 keeps one */
#define THM_W_BUF        0x2b0u       /* the file thread's transfer buffer, and its size */
#define THM_W_SIZE       0x2b4u
#define THM_PR_TEXT      0xd4u        /* the two fields of an XMLPrinter 0x0808ed14 reads */
#define THM_PR_LEN       0xf0u        /* (the text's length + 1) */
#define THM_PR_BYTES     0xf8u
#define THM_TXT_BYTES    1024u
#define THM_DISPLAY      0x24002e7cu  /* the display object: its DMA2D handle at +0xa8 */
#define THM_DMA2D_H      0xa8u
#define THM_DMA2D        0x52001000u
#define THM_UI_PTR       0x24000dc8u  /* the UI task's pointer to the UI object */
#define THM_UI_DIRTY     0x2b8u       /* its root widget (+0x280)'s dirty bytes, bottom and top layer */

typedef struct {
    uint32_t magic;
    uint32_t req;                    /* the Settings row's value, as last posted */
    uint32_t cmagic;                 /* THM_CMAGIC once the rest is set */
    uint32_t flags;                  /* THM_F_FILE, THM_F_WRITE, THM_F_RELOAD */
    uint32_t col[THM_FAM];           /* Custom: 0xffRRGGBB, or 0 for Stock's */
#ifdef THM_SAFE
    uint32_t t_save;                 /* the file thread: when the last settings save started */
    uint32_t asked;                  /* the UI: the t_save it last posted a save after */
#endif
} thm_ram_t;                         /* 96 bytes, 104 with THM_SAFE (patch_micro.py: THEME_BYTES) */

/* theme.tml's names, in the maker's order, and the one each palette entry
 * takes (THEMES.md: the families) */
static const char *const thm_names[THM_FAM] = {
    "background", "black", "panel", "grey1", "cell", "grey3", "grey4", "grey5", "text", "text-bright",
    "selection", "accent", "accent-dark", "red", "red-dark", "green", "yellow", "yellow-dark", "teal", "purple",
};
static const uint8_t thm_fam[THM_N] = {
    255u, 4u, 0u, 8u, 10u, 11u, 13u, 11u, 0u, 5u, 11u, 15u, 14u, 0u, 1u, 9u, 3u, 6u,
    15u, 14u, 16u, 14u, 7u, 18u, 19u, 2u, 12u, 11u, 17u, 11u, 9u, 11u, 10u, 9u,
};

static const uint32_t thm_tab[THM_BUILT - 1u][THM_N] = {
    {   /* Amber */
        0x00000000u, 0xff5a4c3bu, 0xff16120cu, 0xfffff3e0u, 0xffa8401cu, 0xffffb000u,
        0xffff3b1fu, 0xffffb000u, 0xff16120cu, 0xff6b5b47u, 0xffffb000u, 0xff9cc72bu,
        0xffb8321cu, 0xff16120cu, 0xff000000u, 0xfffff8ecu, 0xff463b2eu, 0xff776650u,
        0xff9cc72bu, 0xffb8321cu, 0xffffe066u, 0xffb8321cu, 0xffb8a58cu, 0xffffd27au,
        0xffff8a3du, 0xff2c241au, 0xff7a5200u, 0xffffb000u, 0xff7a6a1eu, 0xffffb000u,
        0xfffff8ecu, 0xffffb000u, 0xffa8401cu, 0xfffff8ecu,
    },
    {   /* Stage (b27: redder, less maroon) */
        0x00000000u, 0xff56221eu, 0xff0e0605u, 0xfff0cfc4u, 0xffc8231au, 0xffff9a3du,
        0xffff2a14u, 0xffff9a3du, 0xff0e0605u, 0xff6a2a24u, 0xffff9a3du, 0xffd9a93fu,
        0xffb0241au, 0xff0e0605u, 0xff000000u, 0xfff8dacfu, 0xff461c19u, 0xff76302au,
        0xffd9a93fu, 0xffb0241au, 0xffffc15au, 0xffb0241au, 0xffd27a6cu, 0xffff7a5cu,
        0xffd04a7au, 0xff240f0du, 0xff9a2018u, 0xffff9a3du, 0xff6b4c20u, 0xffff9a3du,
        0xfff8dacfu, 0xffff9a3du, 0xffc8231au, 0xfff8dacfu,
    },
    {   /* Contrast */
        0x00000000u, 0xff3a3a3au, 0xff0e0e0eu, 0xffffffffu, 0xff1f5fd6u, 0xffffb000u,
        0xffff7a00u, 0xffffb000u, 0xff0e0e0eu, 0xff707070u, 0xffffb000u, 0xff3d9bffu,
        0xffb85600u, 0xff0e0e0eu, 0xff000000u, 0xffffffffu, 0xff303030u, 0xff7e7e7eu,
        0xff3d9bffu, 0xffb85600u, 0xffffe14du, 0xffb85600u, 0xffc4c4c4u, 0xff9ad0ffu,
        0xffffb0d8u, 0xff2a2a2au, 0xff7a5600u, 0xffffb000u, 0xff7a6c25u, 0xffffb000u,
        0xffffffffu, 0xffffb000u, 0xff1f5fd6u, 0xffffffffu,
    },
    {   /* Phosphor */
        0x00000000u, 0xff274830u, 0xff07110au, 0xffd8ffe2u, 0xff1c7a36u, 0xffffc43du,
        0xffff4a3au, 0xffffc43du, 0xff07110au, 0xff2e5539u, 0xffffc43du, 0xff9aff50u,
        0xffb33a2eu, 0xff07110au, 0xff000000u, 0xffe8ffeeu, 0xff1e3825u, 0xff356141u,
        0xff9aff50u, 0xffb33a2eu, 0xfff5ff5au, 0xffb33a2eu, 0xff72a882u, 0xff7affc8u,
        0xffc2ff3au, 0xff132418u, 0xff7a5a12u, 0xffffc43du, 0xff6e7a1eu, 0xffffc43du,
        0xffe8ffeeu, 0xffffc43du, 0xff1c7a36u, 0xffe8ffeeu,
    },
    {   /* Synthwave */
        0x00000000u, 0xff4d3961u, 0xff130d19u, 0xfffff0ffu, 0xffb8166fu, 0xff2ee6ffu,
        0xffff3b3bu, 0xff2ee6ffu, 0xff130d19u, 0xff59436fu, 0xff2ee6ffu, 0xff3dffb0u,
        0xffb82a3au, 0xff130d19u, 0xff000000u, 0xfffff6ffu, 0xff3d2d4fu, 0xff644c7bu,
        0xff3dffb0u, 0xffb82a3au, 0xffffe45cu, 0xffb82a3au, 0xffaa98c2u, 0xffae85ffu,
        0xffff8ae0u, 0xff291d35u, 0xff0e6e7au, 0xff2ee6ffu, 0xff7a6a2au, 0xff2ee6ffu,
        0xfffff6ffu, 0xff2ee6ffu, 0xffb8166fu, 0xfffff6ffu,
    },
};

#ifdef CHR_HOST
static uint32_t thm_x_cfgclut(void *h, uint32_t clut, uint32_t mode, uint32_t size, uint32_t layer)
{ (void)h; (void)clut; (void)mode; (void)size; (void)layer; return 0; }
static uint32_t thm_x_enclut(void *h, uint32_t layer) { (void)h; (void)layer; return 0; }
static uint32_t thm_x_wait(void *h, uint32_t ms) { (void)h; (void)ms; return 0; }
static uint32_t thm_x_fopen(void *f, const char *p, uint32_t m) { (void)f; (void)p; (void)m; return 4; }
static uint32_t thm_x_fclose(void *f) { (void)f; return 0; }
static void thm_x_sdcheck(void *sd) { (void)sd; }
static void thm_x_doc(void *d, uint32_t e, uint32_t w) { (void)d; (void)e; (void)w; }
static void thm_x_undoc(void *d) { (void)d; }
static uint32_t thm_x_loadfile(const char *p, void *b, uint32_t n, void *d) { (void)p; (void)b; (void)n; (void)d; return 0; }
static void *thm_x_child(void *n, const char *name) { (void)n; (void)name; return 0; }
static const char *thm_x_attr(void *e, const char *name, const char *v) { (void)e; (void)name; (void)v; return 0; }
static uint32_t thm_x_savefile(const char *p, void *pr, void *b, uint32_t n) { (void)p; (void)pr; (void)b; (void)n; return 0; }
static void *thm_x_malloc(uint32_t n) { (void)n; return 0; }
static void thm_x_free(void *a) { (void)a; }
#else
extern uint32_t thm_x_cfgclut(void *h, uint32_t clut, uint32_t mode, uint32_t size,
                              uint32_t layer);        /* 0x0804a8b0: HAL ConfigCLUT, the config by value */
extern uint32_t thm_x_enclut(void *h, uint32_t layer); /* 0x0804a500: HAL EnableCLUT, starts the load */
extern uint32_t thm_x_wait(void *h, uint32_t ms);      /* 0x0804a536: waits for it */
extern uint32_t thm_x_fopen(void *fil, const char *path, uint32_t mode);   /* 0x08056edc: FatFs f_open */
extern uint32_t thm_x_fclose(void *fil);                                    /* 0x080576a6: f_close */
extern void thm_x_sdcheck(void *sd);       /* 0x0804440c: the card checked, remounted if it fails (b38) */
extern void thm_x_doc(void *doc, uint32_t entities, uint32_t ws);           /* 0x0809e50c: XMLDocument() */
extern void thm_x_undoc(void *doc);                                         /* 0x0809d06c: ~XMLDocument() */
extern uint32_t thm_x_loadfile(const char *path, void *buf, uint32_t n, void *doc);   /* 0x0808edc4: read, parse */
extern void *thm_x_child(void *node, const char *name);                     /* 0x0809d608: FirstChildElement */
extern const char *thm_x_attr(void *el, const char *name, const char *v);   /* 0x0809dca8: Attribute */
extern uint32_t thm_x_savefile(const char *path, void *pr, void *buf, uint32_t n);    /* 0x0808ed14: write */
extern void *thm_x_malloc(uint32_t n);                                      /* 0x08042e58 */
extern void thm_x_free(void *a);                                            /* 0x08042ebc */
#endif

/* the colour theme t gives palette entry i */
static uint32_t thm_colour(uint32_t t, uint32_t i)
{
    uint32_t s = ((const volatile uint32_t *)(uintptr_t)THM_PAL_FLASH)[i];
    if (i == 0u || i >= THM_N || t == 0u || t >= THM_COUNT) return s;
    if (t < THM_BUILT) return thm_tab[t - 1u][i];
    const volatile thm_ram_t *r = (const volatile thm_ram_t *)(uintptr_t)THM_RAM;   /* Custom */
    uint32_t c = r->cmagic == THM_CMAGIC ? r->col[thm_fam[i]] : 0u;
    return c ? (c | 0xff000000u) : s;
}

#ifdef THM_NOW
/* the palette for theme `want` into RAM, the CLUT and the screen: the UI
 * tick's code (below, without THM_NOW), shared by thm_post (THM_NOW) */
static void thm_apply(uint32_t want)
{
    volatile uint32_t *pal = (volatile uint32_t *)(uintptr_t)THM_PAL;
    int same = 1;
    for (uint32_t i = 0; i < THM_N; i++) same &= pal[i] == thm_colour(want, i);
    if (same) return;
    uint8_t *disp = (uint8_t *)(uintptr_t)THM_DISPLAY;
    void *h = disp + THM_DMA2D_H;
    if (*(volatile uint32_t *)h != THM_DMA2D) return;   /* not the blitter this was traced against */
    for (int n = 0; n < 200000 && (*(volatile uint32_t *)(uintptr_t)THM_DMA2D & 1u); n++) { }   /* a blit in flight */
    for (uint32_t i = 0; i < THM_N; i++) pal[i] = thm_colour(want, i);
    for (uint32_t a = THM_PAL & ~31u; a < THM_PAL + 4u * THM_N; a += 32u)   /* b22: out to RAM first */
        *(volatile uint32_t *)(uintptr_t)0xE000EF68u = a;
    __asm volatile ("dsb sy" ::: "memory");
    for (uint32_t layer = 0; layer < 2u; layer++) {   /* as boot loads it (0x080425e0) */
        thm_x_cfgclut(h, THM_PAL, 0u, THM_N - 1u, layer);
        thm_x_wait(h, 100u);
        thm_x_enclut(h, layer);
        thm_x_wait(h, 100u);
    }
    uint32_t ui = *(volatile const uint32_t *)(uintptr_t)THM_UI_PTR;
    if (ui >= 0x20000000u && ui < 0x24080000u && !(ui & 3u)) {   /* repaint everything */
        *(volatile uint8_t *)(uintptr_t)(ui + THM_UI_DIRTY) = 1u;
        *(volatile uint8_t *)(uintptr_t)(ui + THM_UI_DIRTY + 1u) = 1u;
    }
}
#endif

/* Hook T, at boot and on every edit of the Settings row: the theme wanted.
 * Custom chosen with no theme.tml: the colours on screen, to be written out;
 * with one (b37): it is read again at this edit's settings save. */
void thm_post(uint32_t v)
{
    thm_ram_t *t = (thm_ram_t *)(uintptr_t)THM_RAM;
    v = v < THM_COUNT ? v : 0u;
#ifdef THM_SAFE
    int boot = t->cmagic == THM_CMAGIC && (t->flags & THM_F_BOOT);
    if (boot) t->flags &= ~THM_F_BOOT;
#else
    const int boot = 0;
#endif
    if (!boot && v == THM_CUSTOM && t->magic == THM_MAGIC && t->req != THM_CUSTOM
        && t->cmagic == THM_CMAGIC && (t->flags & THM_F_FILE))
        t->flags |= THM_F_RELOAD;
    if (v == THM_CUSTOM && t->magic == THM_MAGIC && t->req != THM_CUSTOM
        && !(t->cmagic == THM_CMAGIC && (t->flags & THM_F_FILE))) {
        uint32_t was = t->req < THM_COUNT ? t->req : 0u;
        for (uint32_t f = 0; f < THM_FAM; f++) {
            uint32_t i = 1u;
            while (i < THM_N && thm_fam[i] != f) i++;
            t->col[f] = thm_colour(was, i) | 0xff000000u;
        }
        t->flags = THM_F_WRITE;
        t->cmagic = THM_CMAGIC;
    }
    t->req = v;
    t->magic = THM_MAGIC;
#ifdef THM_NOW
    thm_apply(v);                    /* the UI thread, before this edit's repaint */
#endif
}

/* "#RRGGBB" (or without the #, spaces either side): 0xffRRGGBB; else 0 */
static uint32_t thm_hex(const char *s)
{
    if (!s) return 0u;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '#') s++;
    uint32_t v = 0u;
    for (int k = 0; k < 6; k++, s++) {
        char ch = *s;
        uint32_t d = ch >= '0' && ch <= '9' ? (uint32_t)(ch - '0') : ch >= 'a' && ch <= 'f' ? (uint32_t)(ch - 'a' + 10)
                   : ch >= 'A' && ch <= 'F' ? (uint32_t)(ch - 'A' + 10) : 16u;
        if (d > 15u) return 0u;
        v = v << 4 | d;
    }
    while (*s == ' ' || *s == '\t') s++;
    return *s ? 0u : (v | 0xff000000u);
}

static const char thm_path[] = "theme.tml";

/* the file thread, before a settings load: theme.tml's colours, if it is there;
 * again (b37, before a settings save): only if it is, else as they were */
static void thm_load(uint8_t *w, int again)
{
    thm_ram_t *t = (thm_ram_t *)(uintptr_t)THM_RAM;
    uint32_t col[THM_FAM];
    for (uint32_t f = 0; f < THM_FAM; f++) col[f] = 0u;
    uint8_t *fil = *(uint8_t *volatile *)(uintptr_t)THM_FIL_PTR;
    int file = 0;
    /* b38: a card swapped while running is not seen until something remounts
     * it -- stock's preset jobs start with this check (0x0808c8a2,
     * 0x0808d194), which opens the root and, failing, remounts the card */
#if !defined(THM_SAFE) || defined(THM_HOTSWAP)
    if (again) thm_x_sdcheck((void *)(uintptr_t)THM_SDCARD);   /* THM_HOTSWAP: settled (thm_job) */
#endif
    if (fil) {
        for (uint32_t k = 0; k < THM_FIL_BYTES; k++) fil[k] = 0u;
        file = thm_x_fopen(fil, thm_path, 1u) == 0u;   /* FA_READ: is it there? */
        if (file) thm_x_fclose(fil);
#if defined(THM_SAFE) && !defined(THM_HOTSWAP)
        if (again && !file) {        /* not there: perhaps a card swapped -- check it, then once more */
            thm_x_sdcheck((void *)(uintptr_t)THM_SDCARD);
            for (uint32_t k = 0; k < THM_FIL_BYTES; k++) fil[k] = 0u;
            file = thm_x_fopen(fil, thm_path, 1u) == 0u;
            if (file) thm_x_fclose(fil);
        }
#endif
    }
    if (again && !file) return;
    if (file) {
        uint64_t doc[THM_DOC_BYTES / 8u];
        thm_x_doc(doc, 1u, 0u);
        if (thm_x_loadfile(thm_path, *(void **)(w + THM_W_BUF), *(uint32_t *)(w + THM_W_SIZE), doc)) {
            void *e = thm_x_child(doc, "document");
            if (e) e = thm_x_child(e, "theme");
            if (e)
                for (uint32_t f = 0; f < THM_FAM; f++) col[f] = thm_hex(thm_x_attr(e, thm_names[f], 0));
        }
        thm_x_undoc(doc);
    }
    for (uint32_t f = 0; f < THM_FAM; f++) t->col[f] = col[f];
    t->flags = file ? THM_F_FILE : 0u;
#ifdef THM_SAFE
    if (!again) {                    /* boot: no save yet, and boot's thm_post is next */
        t->flags |= THM_F_BOOT;
        t->t_save = osd_now();
        t->asked = t->t_save;
    }
#endif
    t->cmagic = THM_CMAGIC;
}

static char *thm_put(char *o, const char *s) { while (*s) *o++ = *s++; return o; }

/* the file thread, before a settings save: the starter theme.tml, if wanted */
static void thm_write(uint8_t *w)
{
    thm_ram_t *t = (thm_ram_t *)(uintptr_t)THM_RAM;
    uint8_t *m = thm_x_malloc(THM_PR_BYTES + THM_TXT_BYTES);
    if (!m) return;
    char *txt = (char *)m + THM_PR_BYTES, *o = txt;
    static const char hx[] = "0123456789ABCDEF";
    o = thm_put(o, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                   "<!-- A Bitbox Micro theme, written by the module from the theme on screen.\n"
                   "     Change any colour (#RRGGBB), then set Theme to another and back\n"
                   "     to Custom. A colour that is missing or mistyped shows as Stock's,\n"
                   "     so the screen stays readable. -->\n"
                   "<document>\n    <theme name=\"Custom\"\n");
    for (uint32_t f = 0; f < THM_FAM; f++) {
        uint32_t c = t->col[f];
        o = thm_put(o, "        ");
        o = thm_put(o, thm_names[f]);
        o = thm_put(o, "=\"#");
        for (int k = 20; k >= 0; k -= 4) *o++ = hx[(c >> k) & 15u];
        o = thm_put(o, "\"\n");
    }
    o = thm_put(o, "    />\n</document>\n");
    *o = 0;
    for (uint32_t k = 0; k < THM_PR_BYTES; k++) m[k] = 0u;
    *(char **)(m + THM_PR_TEXT) = txt;
    *(uint32_t *)(m + THM_PR_LEN) = (uint32_t)(o - txt) + 1u;
    if (thm_x_savefile(thm_path, m, *(void **)(w + THM_W_BUF), *(uint32_t *)(w + THM_W_SIZE)))
        t->flags = THM_F_FILE;
    thm_x_free(m);
}

/* Hook T, on the file thread (w: its object), before its settings load
 * (kind 0) or save (1) */
void thm_job(uint8_t *w, uint32_t kind)
{
    thm_ram_t *t = (thm_ram_t *)(uintptr_t)THM_RAM;
#ifdef THM_SAFE
    if (!kind) { thm_load(w, 0); return; }
    uint32_t now = osd_now(), last = t->t_save;
    t->t_save = now;                 /* this save's write follows at once */
    if (t->cmagic == THM_CMAGIC && (t->flags & THM_F_RELOAD)) {
        if (t->req != THM_CUSTOM) t->flags &= ~THM_F_RELOAD;          /* moved off Custom since */
        else if (now - last >= THM_SETTLE) {
            t->flags &= ~THM_F_RELOAD;
            thm_load(w, 1);
        }                            /* else too soon after the last save: the UI tick asks again */
    } else if (t->cmagic == THM_CMAGIC && (t->flags & THM_F_WRITE)) thm_write(w);
#else
    if (!kind) thm_load(w, 0);
    else if (t->cmagic == THM_CMAGIC && (t->flags & THM_F_RELOAD)) {
        t->flags &= ~THM_F_RELOAD;
        thm_load(w, 1);
    } else if (t->cmagic == THM_CMAGIC && (t->flags & THM_F_WRITE)) thm_write(w);
#endif
}

/* b24: the patch's own colours follow the theme -- the pop-up box (C) and
 * hook K's CPU bar and hook M's tempo strips (assembly). The top layer is
 * ARGB1555, so they are kept here ready to store, in patch RAM, the layout
 * the hooks read: a magic, then THO_N halfwords. Stock keeps 2.3.9's colours
 * exactly, and so do the pop-up box in every theme (b27) and the CPU bar in
 * every theme but Custom (b33); a theme takes the rest from the palette entry
 * of its family (Custom's bar: green/amber/red -> green/yellow/red; the strips'
 * grey/blue/green -> grey4/accent/green). Until the first tick
 * fills it the hooks use their own copy of Stock's. */
#define THO_RAM          0x2406c0e0u  /* patch_micro.py: THEME_OSD_RAM */
#define THO_MAGIC        0x54484f31u  /* "THO1" */
typedef struct {
    uint32_t magic;
    uint16_t c[THO_N];
} tho_ram_t;
static const uint16_t tho_stock[THO_N] = {   /* patch_micro.py: THEME_OSD_STOCK */
    0x8842u, 0xffffu, 0x83e0u, 0xfe80u, 0xfc00u, 0xffffu, 0x8842u, 0xc210u, 0x801fu, 0x83e0u, 0x8000u, 0xffffu, 0xfc00u,
};
static const uint8_t tho_entry[THO_N] = { 2u, 3u, 11u, 20u, 6u, 15u, 2u, 17u, 5u, 11u, 14u, 15u, 6u };

static uint16_t tho_1555(uint32_t c)
{
    return (uint16_t)(0x8000u | ((c >> 19) & 31u) << 10 | ((c >> 11) & 31u) << 5 | ((c >> 3) & 31u));
}

/* the colour for role r now (the C's own reads: the pop-up box) */
uint16_t thm_osd_col(uint32_t r, uint16_t dflt)
{
    const volatile tho_ram_t *o = (const volatile tho_ram_t *)(uintptr_t)THO_RAM;
    return o->magic == THO_MAGIC && r < THO_N ? o->c[r] : dflt;
}

static void tho_update(uint32_t want)
{
    volatile tho_ram_t *o = (volatile tho_ram_t *)(uintptr_t)THO_RAM;
    for (uint32_t r = 0; r < THO_N; r++) {
        /* b27, the owner: the pop-up stays Stock's white on black in every
         * theme, so it always stands apart from the screen behind it; b33, the
         * owner: the CPU bar keeps Stock's green/amber/red too, except in Custom */
        int stock = want == 0u || r <= THO_BOX_FG || (r <= THO_BAR_BG && want != THM_CUSTOM);
        uint16_t v = stock ? tho_stock[r] : tho_1555(thm_colour(want, tho_entry[r]));
        if (o->c[r] != v) o->c[r] = v;
    }
    o->magic = THO_MAGIC;
}

/* UI side, every tick: if the palette is not the theme wanted (a new choice,
 * or boot's copy after a restart that kept patch RAM), make it so */
static void thm_ui_tick(void)
{
    const thm_ram_t *t = (const thm_ram_t *)(uintptr_t)THM_RAM;
    if (t->magic != THM_MAGIC) return;               /* no setting posted yet: boot is still loading */
#ifdef THM_SAFE
    /* a re-read waiting for the card to settle: one more settings save, once,
     * THM_SETTLE after the last (cmp_ui_tick's post, 0x0808c154) */
    if (t->cmagic == THM_CMAGIC && (t->flags & THM_F_RELOAD) && t->req == THM_CUSTOM
        && t->asked != t->t_save && osd_now() - t->t_save >= THM_SETTLE) {
        ((thm_ram_t *)(uintptr_t)THM_RAM)->asked = t->t_save;
        cmp_x_save((void *)(uintptr_t)CMP_SAVE_MGR, (const char *)(uintptr_t)CMP_SETTINGS_TML,
                   (void *)(uintptr_t)CMP_GLOBAL_STORE);
    }
#endif
    uint32_t want = t->req < THM_COUNT ? t->req : 0u;
    tho_update(want);
#ifdef THM_NOW
    thm_apply(want);
}
#else
    volatile uint32_t *pal = (volatile uint32_t *)(uintptr_t)THM_PAL;
    int same = 1;
    for (uint32_t i = 0; i < THM_N; i++) same &= pal[i] == thm_colour(want, i);
    if (same) return;
    uint8_t *disp = (uint8_t *)(uintptr_t)THM_DISPLAY;
    void *h = disp + THM_DMA2D_H;
    if (*(volatile uint32_t *)h != THM_DMA2D) return;   /* not the blitter this was traced against */
    for (int n = 0; n < 200000 && (*(volatile uint32_t *)(uintptr_t)THM_DMA2D & 1u); n++) { }   /* a blit in flight */
    for (uint32_t i = 0; i < THM_N; i++) pal[i] = thm_colour(want, i);
    /* b22: the palette is in cached RAM (boot turns the data cache on at
     * 0x080411f6 and no MPU region covers 0x24000000), and the DMA2D loads
     * the CLUT from RAM itself. b20-b21 loaded what RAM still held -- the
     * theme before -- so every change showed the one chosen before it. Clean
     * the palette's lines out to RAM first (SCB DCCMVAC). */
    for (uint32_t a = THM_PAL & ~31u; a < THM_PAL + 4u * THM_N; a += 32u)
        *(volatile uint32_t *)(uintptr_t)0xE000EF68u = a;
    __asm volatile ("dsb sy" ::: "memory");
    for (uint32_t layer = 0; layer < 2u; layer++) {   /* as boot loads it (0x080425e0) */
        thm_x_cfgclut(h, THM_PAL, 0u, THM_N - 1u, layer);
        thm_x_wait(h, 100u);
        thm_x_enclut(h, layer);
        thm_x_wait(h, 100u);
    }
    uint32_t ui = *(volatile const uint32_t *)(uintptr_t)THM_UI_PTR;
    if (ui >= 0x20000000u && ui < 0x24080000u && !(ui & 3u)) {   /* repaint everything */
        *(volatile uint8_t *)(uintptr_t)(ui + THM_UI_DIRTY) = 1u;
        *(volatile uint8_t *)(uintptr_t)(ui + THM_UI_DIRTY + 1u) = 1u;
    }
}
#endif

#endif
