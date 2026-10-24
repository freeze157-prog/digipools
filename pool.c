/* SPDX-License-Identifier: GPL-2.0-or-later */
/* DigiPools: A multi-sample pool machine for the Digitakt MK1.
 *
 * Desired behavior:
 * - A new SRC machine, DIGIPOOLS (ID 10), based on ONESHOT.
 * - In Chromatic mode (FUNC + TRK), each trig key / note plays a distinct
 *   sample from the project's RAM sample pool (1..127).
 * - To select what key plays what sample: in Chromatic mode, keep a key
 *   pressed, and in the SRC page use encoder D (SAMP) to select the sample
 *   for that key.
 * - While held, the OLED display gives instant visual feedback:
 *   Knob D formats as "K<key>:<slot>" and an on-screen banner indicates
 *   "KEY <key> -> SMP <slot>".
 * - When live recording or step recording, each trig is recorded with a
 *   parameter lock of SAMP and root pitch (NOTE 60), ensuring identical
 *   playback from the sequencer.
 */

#include "os154.h"

#include "digitakt-mk1/core3.h"

typedef unsigned char u8;
typedef signed char s8;
typedef short s16;
typedef unsigned short u16;
typedef unsigned int u32;
typedef int s32;

#define DIGIPOOLS_MACHINE 10
#define SLOTS             128
#define TRACKS            8
#define K_TRIG1           24
#define K_SRC             20
#define SRC_KEYS          0x40
#define S_MACHINE         0x7e
#define S_TUNE            0x36
#define S_SAMP            0x3c
#define P_TUNE            17
#define P_SAMP            20

/* Stock firmware routines */
typedef u32 (*fn0_t)(void);
typedef u32 (*fn1_t)(u32);
typedef u32 (*fn2_t)(u32, u32);
typedef s32 (*lockset_t)(u32 trk, s32 step, s32 slot, s32 value);
typedef s32 (*noteset_t)(u32 trk, s32 step, s32 note);

#define APP      ((fn0_t)OS_APP)
#define TSTATE   ((fn1_t)OS_TSTATE)
#define KB_ON    ((fn1_t)OS_KB_ON)
#define ACTIVE   ((fn1_t)OS_ACTIVE)
#define KIT      ((fn1_t)OS_KIT)
#define SOUND    ((fn2_t)OS_SOUND)
#define PATTERN  ((fn1_t)OS_PATTERN)
#define PTRACK   ((fn2_t)OS_PTRACK)
#define LOCKSET  ((lockset_t)OS_LOCKSET)
#define NOTESET  ((noteset_t)OS_NOTESET)
#define LOCKLIST ((fn0_t)OS_LOCKLIST)
#define PREVIEW  (*(volatile u32 *)OS_PREVIEW)
#define KB_CALL  OS_KB_CALL

/* Object data accessor: slot 10 in vtable */
static u8 *odata(u32 obj)
{
    u32 (*get)(u32);
    if (!obj) return 0;
    get = *(u32 (**)(u32))(*(u32 *)obj + 40);
    return (u8 *)get(obj);
}

/* Mapping state per track */
static u8 dpl_trig_sample[TRACKS][16];
static u8 dpl_note_sample[TRACKS][128];
static u8 last_sample_played[TRACKS][128];
static u8 dpl_inited[TRACKS];

/* Semitone (0..11 relative to C) to default Trig key index (0..15) */
static const u8 semitone_to_trig[12] = {
    8,  /* 0: C   -> Trig 9  (index 8) */
    0,  /* 1: C#  -> Trig 1  (index 0) */
    9,  /* 2: D   -> Trig 10 (index 9) */
    1,  /* 3: D#  -> Trig 2  (index 1) */
    10, /* 4: E   -> Trig 11 (index 10) */
    11, /* 5: F   -> Trig 12 (index 11) */
    2,  /* 6: F#  -> Trig 3  (index 2) */
    12, /* 7: G   -> Trig 13 (index 12) */
    3,  /* 8: G#  -> Trig 4  (index 3) */
    13, /* 9: A   -> Trig 14 (index 13) */
    4,  /* 10: A# -> Trig 5  (index 4) */
    14, /* 11: B  -> Trig 15 (index 14) */
};

/* Live UI state */
static s32 held_trig = -1;
static s32 held_note = -1;
static u16 held_keys_mask = 0;
static u8  ui_need_redraw = 0;
static u32 kb_notes[TRACKS][4];                 /* 128 bits: covers notes 0..127 */

/* Helper: Check if keyboard/chromatic mode is active */
static s32 is_chromatic_on(void)
{
    u32 app = APP();
    u32 ts;
    if (!app) return 0;
    ts = TSTATE(app);
    if (!ts) return 0;
    return (KB_ON(ts) & 0xff) != 0;
}

/* Helper: Get sound data for track t if it is on DIGIPOOLS machine */
static const u8 *dpl_sound(s32 t)
{
    u32 app;
    const u8 *snd;
    if (t < 0 || t >= TRACKS)
        return 0;
    app = APP();
    if (!app) return 0;
    snd = odata(SOUND(KIT(app), t));
    return (snd && snd[S_MACHINE] == DIGIPOOLS_MACHINE) ? snd : 0;
}

/* Helper: Check if active track is DigiPools */
static s32 is_digipools_active(void)
{
    return dpl_sound((s32)fw_active_track) != 0;
}

/* Initialize default sample mapping for track t (key k -> sample k+1) */
static void dpl_init_track(s32 t)
{
    s32 k;
    if (t < 0 || t >= TRACKS) return;
    if (dpl_inited[t]) return;
    for (k = 0; k < 16; k++)
        dpl_trig_sample[t][k] = (u8)(k + 1);
    for (k = 0; k < 128; k++) {
        u8 s = dpl_trig_sample[t][semitone_to_trig[k % 12]];
        dpl_note_sample[t][k] = s;
        last_sample_played[t][k] = s;
    }
    dpl_inited[t] = 1;
}

/* Get sample assigned to trig key k on track t */
static s32 dpl_get_trig_sample(s32 t, s32 k)
{
    if (t < 0 || t >= TRACKS) return 1;
    dpl_init_track(t);
    if (k < 0 || k >= 16) return 1;
    return dpl_trig_sample[t][k] ? dpl_trig_sample[t][k] : 1;
}

/* Get sample assigned to note n on track t */
static s32 dpl_get_note_sample(s32 t, s32 n)
{
    if (t < 0 || t >= TRACKS) return 1;
    dpl_init_track(t);
    if (n < 0 || n >= 128) return 1;
    if (last_sample_played[t][n])
        return last_sample_played[t][n];
    return dpl_trig_sample[t][semitone_to_trig[n % 12]];
}

/* Note tracking for recording */
static s32 kb_note_track(s32 track, s32 note, s32 set)
{
    u32 b, m, *w;
    if (track < 0 || track >= TRACKS || note < 0 || note > 127)
        return 0;
    b = (u32)note;
    m = 1u << (b & 31);
    w = &kb_notes[track][b >> 5];
    s32 was = (*w & m) != 0;
    if (set)
        *w |= m;
    else
        *w &= ~m;
    return was;
}

static s32 kb_was_recorded(const u8 *msg, s32 track, s32 note)
{
    return *(const s32 *)(msg + 12) == SRC_KEYS && note >= 0 && note <= 127
        && track >= 0 && track < TRACKS && kb_note_track(track, note, 0)
        && dpl_sound(track) != 0;
}

/* ---- NOTEON Intercept (called from glue.s dpl_kbnote) ---------------------------- */
void dpl_kb_noteon(s32 track, s32 note, u8 *fp, u32 audio_a, u32 audio_b)
{
    u8 *l;
    s32 key;
    const u8 *snd;
    s32 samp;

    if (*(s32 *)(fp + 20) != SRC_KEYS || note < 0 || note > 127 || track < 0 || track >= TRACKS)
        return;

    key = (*(u32 *)(fp + 4) == KB_CALL);
    kb_note_track(track, note, key);

    if (!key || PREVIEW <= 63 || *(u32 *)(fp - 8) || !((audio_a | audio_b) & 0xff))
        return;

    snd = dpl_sound(track);
    if (!snd || !is_chromatic_on())
        return;

    /* Remember current held note */
    held_note = note;

    /* Look up sample for this trig / note */
    if (held_trig >= 0 && held_trig < 16)
        samp = dpl_get_trig_sample(track, held_trig);
    else
        samp = dpl_get_note_sample(track, note);

    if (samp <= 0) samp = 1;

    /* Remember exact sample played for this note so recording uses the identical sample */
    last_sample_played[track][note] = (u8)samp;

    /* Allocate voice lock list with 1 lock: slot 20 (SAMP).
     * We preserve natural note pitch and octave up/down so live playing sounds
     * with the selected octave transposition.
     */
    l = (u8 *)LOCKLIST();
    *(s32 *)(l + 8) = 1;                        /* 1 parameter lock: SAMP */
    *(u16 *)(l + 0x14) = P_SAMP;                /* Lock 0: slot 20 (SAMP) */
    *(s16 *)(l + 0x16) = (s16)(samp << 8);

    *(u32 *)(fp - 8) = (u32)l;
}

/* ---- Live Recording Intercept (called from glue.s dpl_kbrec) --------------------- */
void dpl_kb_rec(const u8 *msg)
{
    s32 track = (s8)msg[8], note = (s8)msg[16], step = *(const s16 *)(msg + 0x1c);
    u32 trk;
    const s8 *d;
    s32 samp;

    if (!kb_was_recorded(msg, track, note))
        return;

    if (step < 0)
        step = 0;
    else if (step > 63)
        step = 63;

    trk = PTRACK(PATTERN(APP()), track);
    d = (const s8 *)odata(trk);
    if (!d || d[0x280 + step] != note)
        return;

    /* Retrieve the exact sample played live for this note/key */
    samp = last_sample_played[track][note];
    if (samp <= 0) {
        if (held_trig >= 0 && held_trig < 16)
            samp = dpl_get_trig_sample(track, held_trig);
        else
            samp = dpl_get_note_sample(track, note);
    }
    if (samp <= 0) samp = 1;

    /* Set parameter lock on SAMP for this step */
    LOCKSET(trk, step, P_SAMP, samp << 8);

    /* NOTE is preserved as recorded by REC_NOTE!
     * This keeps the exact pitch and octave up/down played live.
     */
}

/* ---- Step Recording Intercept (called from glue.s dpl_kbstep) -------------------- */
s32 dpl_kb_stepnote(u32 trk, s32 step, s32 note, const u32 *ctx)
{
    s32 track = *(const s32 *)ctx[0];
    if (kb_was_recorded((const u8 *)ctx[1], track, note)) {
        s32 samp = last_sample_played[track][note];
        if (samp <= 0) {
            if (held_trig >= 0 && held_trig < 16)
                samp = dpl_get_trig_sample(track, held_trig);
            else
                samp = dpl_get_note_sample(track, note);
        }
        if (samp <= 0) samp = 1;

        LOCKSET(trk, step, P_SAMP, samp << 8);
        /* Keep note intact to preserve tuning */
    }
    return NOTESET(trk, step, note);
}

/* ---- Key Events: Track held trig keys in Chromatic mode -------------------------- */
s32 dpl_key(void *brain, void *ev)
{
    s32 key = *(const s32 *)((const u8 *)ev + 12);
    s32 fl = *(const s32 *)((const u8 *)ev + 16);
    (void)brain;

    if (key >= K_TRIG1 && key < K_TRIG1 + 16) {
        s32 k = key - K_TRIG1;
        if (fl & 1) {                           /* Key pressed */
            held_keys_mask |= (1u << k);
            held_trig = k;
            if (is_chromatic_on() && is_digipools_active())
                ui_need_redraw = 1;
        } else {                                /* Key released */
            held_keys_mask &= ~(1u << k);
            if (held_trig == k) {
                held_trig = -1;
                /* If another key remains held, switch focus to it */
                for (s32 i = 0; i < 16; i++) {
                    if (held_keys_mask & (1u << i)) {
                        held_trig = i;
                        break;
                    }
                }
            }
            if (is_chromatic_on() && is_digipools_active())
                ui_need_redraw = 1;
        }
    }
    return 0;                                   /* Do not block: allow normal triggering */
}

/* ---- Encoder Events: Select sample for held key on SRC page ---------------------- */
s32 dpl_enc(void *brain, void *ev)
{
    s32 enc = *(const s32 *)((const u8 *)ev + 12);
    s32 delta = *(const s32 *)((const u8 *)ev + 16);
    s32 track = (s32)fw_active_track;
    (void)brain;

    if (held_trig >= 0 && is_chromatic_on() && dpl_sound(track)) {
        if (enc == 4) {                         /* Encoder D = SAMP */
            s32 cur = (s32)dpl_get_trig_sample(track, held_trig);
            cur += delta;
            if (cur < 1) cur = 1;
            if (cur > 127) cur = 127;

            dpl_trig_sample[track][held_trig] = (u8)cur;

            /* Update all octave notes that correspond to this trig key */
            for (s32 n = 0; n < 128; n++) {
                if (semitone_to_trig[n % 12] == held_trig) {
                    dpl_note_sample[track][n] = (u8)cur;
                    last_sample_played[track][n] = (u8)cur;
                }
            }
            if (held_note >= 0 && held_note < 128) {
                dpl_note_sample[track][held_note] = (u8)cur;
                last_sample_played[track][held_note] = (u8)cur;
            }

            ui_need_redraw = 1;
            return 1;                           /* Consumed: do not modify track's base SAMP */
        }
    }
    return 0;
}

/* ---- Screen Drawing: On-screen feedback when editing key's sample ---------------- */
void dpl_draw(void *bmp, void *ctrl)
{
    s32 track = (s32)fw_active_track;

    if (ui_need_redraw && ctrl) {
        *((u8 *)ctrl + 0x20) = 1;               /* Mark screen dirty to re-render */
        ui_need_redraw = 0;
    }

    if (held_trig >= 0 && is_chromatic_on() && dpl_sound(track)) {
        s32 slot = (s32)dpl_get_trig_sample(track, held_trig);
        char msg[24];
        char *p = msg;
        s32 v = held_trig + 1;

        /* Box coordinates: x 14..114, y 52..62 (top of 128x64 display) */
        fw_fillrect(bmp, 14, 52, 114, 62, 0);   /* Clear box */
        fw_framerect(bmp, 14, 52, 114, 62, 1);  /* Draw outline */

        /* Format "KEY X -> SMP Y" without standard sprintf */
        *p++ = 'K'; *p++ = 'E'; *p++ = 'Y'; *p++ = ' ';
        if (v >= 10) *p++ = (char)('0' + v / 10);
        *p++ = (char)('0' + (v % 10));
        *p++ = ' '; *p++ = '-'; *p++ = '>'; *p++ = ' ';
        *p++ = 'S'; *p++ = 'M'; *p++ = 'P'; *p++ = ' ';
        if (slot >= 100) {
            *p++ = (char)('0' + slot / 100);
            slot %= 100;
            *p++ = (char)('0' + slot / 10);
        } else if (slot >= 10) {
            *p++ = (char)('0' + slot / 10);
        }
        *p++ = (char)('0' + (slot % 10));
        *p = 0;

        fw_textf(bmp, (const void *)OS_FONT5, 20, 54, 20, msg);
    }
}

/* ---- Periodic Tick: Clean up state if Chromatic mode is turned off --------------- */
void dpl_tick(u8 *ctrl)
{
    (void)ctrl;
    if (!is_chromatic_on()) {
        if (held_trig >= 0 || held_keys_mask != 0) {
            held_trig = -1;
            held_keys_mask = 0;
            held_note = -1;
            ui_need_redraw = 1;
        }
    }
}

/* ---- Knob D Formatter on SRC Page ------------------------------------------------ */
static int32_t pool_samp_fmt(char *buf, int32_t value, int32_t ctx, int32_t machine)
{
    s32 track = (s32)fw_active_track;
    s32 slot, k;
    char *p = buf;
    (void)value;
    (void)machine;

    if (held_trig >= 0 && is_chromatic_on() && dpl_sound(track)) {
        slot = (s32)dpl_get_trig_sample(track, held_trig);
        k = held_trig + 1;

        if (ctx == 0) {
            /* Short label under knob: "K<key>:<slot>" */
            *p++ = 'K';
            if (k >= 10) *p++ = (char)('0' + k / 10);
            *p++ = (char)('0' + (k % 10));
            *p++ = ':';
            if (slot >= 100) {
                *p++ = (char)('0' + slot / 100);
                slot %= 100;
                *p++ = (char)('0' + slot / 10);
            } else if (slot >= 10) {
                *p++ = (char)('0' + slot / 10);
            }
            *p++ = (char)('0' + (slot % 10));
            *p = 0;
            return 1;
        } else {
            /* Pop-up label: "KEY <key>: SMP <slot>" */
            *p++ = 'K'; *p++ = 'E'; *p++ = 'Y'; *p++ = ' ';
            if (k >= 10) *p++ = (char)('0' + k / 10);
            *p++ = (char)('0' + (k % 10));
            *p++ = ':'; *p++ = ' ';
            *p++ = 'S'; *p++ = 'M'; *p++ = 'P'; *p++ = ' ';
            if (slot >= 100) {
                *p++ = (char)('0' + slot / 100);
                slot %= 100;
                *p++ = (char)('0' + slot / 10);
            } else if (slot >= 10) {
                *p++ = (char)('0' + slot / 10);
            }
            *p++ = (char)('0' + (slot % 10));
            *p = 0;
            return 1;
        }
    }
    return 0;                                   /* Default stock formatter */
}

/* ---- SRC Page UI Layout (copies ONESHOT page_from = 0) -------------------------- */
static const struct cm_ui dpl_page = {
    .abi = cm_ui_v3,
    .page_from = 0,                             /* ONESHOT's SRC page */
    .knob = {
        [3] = {                                 /* Knob D: SAMP */
            .fmt = pool_samp_fmt,
        },
    },
};

/* ---- Menu Icon (11 x 7 bitmap, ColdFire big-endian format) ---------------------- */
/* Symbol: Drum Pad Grid / Pool */
static const uint32_t dpl_icon_px[11] = {
    0xfe000000, 0x92000000, 0xfe000000, 0x10000000, 0xfe000000,
    0x92000000, 0xfe000000, 0x10000000, 0xfe000000, 0x92000000,
    0xfe000000,
};

static const uint32_t dpl_icon_mask[11] = {
    0xfe000000, 0xfe000000, 0xfe000000, 0xfe000000, 0xfe000000,
    0xfe000000, 0xfe000000, 0xfe000000, 0xfe000000, 0xfe000000,
    0xfe000000,
};

static const struct {
    const void *vt;
    int32_t width, height, one;
    const uint32_t *px, *mask;
    int32_t zero;
} dpl_icon = { fw_bitmap_vt, 11, 7, 1, dpl_icon_px, dpl_icon_mask, 0 };

/* ---- Machine Descriptor (contributed to core_machines) -------------------------- */
const struct cm_machine dpl_machine = {
    .id = DIGIPOOLS_MACHINE,
    .name = "DIGIPOOLS",
    .sname = "POOL",
    .icon = &dpl_icon,
    .params = 0,                                /* ONESHOT parameter set */
    .render = 0,                                /* ONESHOT stock audio render */
    .tag = CM_UI_TAG,
    .ui = &dpl_page,
};
