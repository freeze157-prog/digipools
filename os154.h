/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Digitakt mk1 MAIN OS 1.54: stock routines and data */
#ifndef DIGIPOOLS_OS154_H
#define DIGIPOOLS_OS154_H

#define OS_SMP_TAB      0x4031A3A0u  /* +16 slot: PCM, rate, length, ratio */
#define OS_REF_TAB      0x421F330Cu  /* +16 slot: the loader's reference; +4 content hash */
#define OS_VOICE_SLOT   0x8000EE20u  /* V(v) + 0x5C, V = 0x8000EDC4 + 94 v */
#define OS_FILLRECT     0x400c1bce   /* (bmp, x0, y0, x1, y1, colour) */
#define OS_FRAMERECT    0x400c19b2   /* ... an outline */
#define OS_TEXTF        0x400c27a4   /* (bmp, rawfont, x, y, maxlen, fmt, ...) */
#define OS_FONT5        0x40200ebc   /* the stock 5-px font */
#define OS_NOTEON       0x400d5604   /* note on(track, note, vel, ...) */
#define OS_NOTEOFF      0x400d5986   /* note off(track, note, src) */
#define OS_MACHINE      0x4002b5d4   /* the SRC page's machine */
#define OS_TRACK_OF     0x4001d24e   /* (view + 116) -> the page's track */
#define OS_APP          0x40138882   /* the app */
#define OS_TSTATE       0x40014d86   /* (app) -> the TrackState */
#define OS_KB_ON        0x4001d4f0   /* (TrackState) -> the keyboard is on */
#define OS_ACTIVE       0x4001d27e   /* (TrackState) -> the active track */
#define OS_KIT          0x40014d92   /* (app) -> the kit */
#define OS_SOUND        0x4000d7be   /* (kit, t) -> track t's sound */
#define OS_PATTERN      0x40015786   /* (app) -> the pattern */
#define OS_PTRACK       0x40012a02   /* (pattern, t) -> its track t */
#define OS_LOCKSET      0x40024768   /* a p-lock on a step */
#define OS_NOTESET      0x40024942   /* a trig's NOTE */
#define OS_LOCKLIST     0x400ee20e   /* a lock list for a voice, emptied */
#define OS_PREVIEW      0x4020c64c   /* the preview step, <= 63 when one */
#define OS_KB_CALL      0x40028bb4   /* NOTEON's return in the keyboard's call */
#define OS_OP_NEW       0x400d43a8   /* operator new(size) */

#endif
