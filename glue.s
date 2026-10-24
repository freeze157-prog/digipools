| SPDX-License-Identifier: GPL-2.0-or-later
| DigiPools glue: ColdFire V4 (MCF54418), Digitakt mk1 OS 1.53 and 1.54.

        .include "os154.inc"

        .section .run, "ax"

| NOTEON hook at 0x400d553e (1.53) / 0x400d5766 (1.54):
| Intercepts NOTEON before voice locks are assigned.
| Registers: d2 = track, d3 = note, fp = NOTEON frame pointer, a4, a3 = audio flags.
        .globl  dpl_kbnote
dpl_kbnote:
        lea     -12(%sp), %sp
        movem.l %d0/%a0-%a1, (%sp)
        move.l  %a3, -(%sp)
        move.l  %a4, -(%sp)
        move.l  %fp, -(%sp)
        move.l  %d3, -(%sp)
        move.l  %d2, -(%sp)
        jsr     dpl_kb_noteon
        lea     20(%sp), %sp
        movem.l (%sp), %d0/%a0-%a1
        lea     12(%sp), %sp
        moveq   #63, %d1                | replaced instruction: moveq #63, %d1
        cmp.l   PREVIEW_STEP, %d1       | replaced instruction: cmpl PREVIEW_STEP, %d1
        rts

| Live recording hook at 0x40008d40 (1.53 & 1.54)
| Replaces jsr 0x4001c10a (REC_NOTE). Records the note, then dpl_kb_rec sets SAMP lock.
        .globl  dpl_kbrec
dpl_kbrec:
        move.l  36(%sp), -(%sp)         | duplicate original 9 arguments
        move.l  36(%sp), -(%sp)
        move.l  36(%sp), -(%sp)
        move.l  36(%sp), -(%sp)
        move.l  36(%sp), -(%sp)
        move.l  36(%sp), -(%sp)
        move.l  36(%sp), -(%sp)
        move.l  36(%sp), -(%sp)
        move.l  36(%sp), -(%sp)
        jsr     REC_NOTE
        lea     36(%sp), %sp
        move.l  %d0, -(%sp)
        move.l  %a2, -(%sp)             | a2 = note message
        jsr     dpl_kb_rec
        addq.l  #4, %sp
        move.l  (%sp)+, %d0
        rts

| Step recording hook at 0x40008904 (1.53 & 1.54)
| Replaces jsr 0x40024942 (NOTESET).
        .globl  dpl_kbstep
dpl_kbstep:
        move.l  %a2, -(%sp)             | ctx
        move.l  16(%sp), -(%sp)         | note
        move.l  16(%sp), -(%sp)         | step
        move.l  16(%sp), -(%sp)         | trk
        jsr     dpl_kb_stepnote
        lea     16(%sp), %sp
        rts
