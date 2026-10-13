;-*-asm-*-
; RUN: --gpu --mem $10000:$20
; Long words stored and loaded at addresses two past a long word's: in Tom's CLUT, 16 bits wide,
; written a half each I/O cycle, and in main RAM; two and six past, the second at a phrase's end.
; The CLUT is read back a word at a time, and long words from the same unaligned addresses
; (Protector clears BORD1 and BORD2 so, at $F0002A).
	gpu
	include "jaghard.inc"
	RUN $F03000
	movei	#$f00400,r14
	moveq	#0,r0
	store	r0,(r14)
	store	r0,(r14+4)
	store	r0,(r14+8)
	store	r0,(r14+12)
	movei	#$11112222,r1
	movei	#$f00402,r2
	store	r1,(r2)
	movei	#$33334444,r1
	movei	#$f0040a,r2
	store	r1,(r2)
	movei	#$55556666,r1
	movei	#$10002,r3
	store	r1,(r3)
	movei	#$f00400,r2
	loadw	(r2),r4
	addq	#2,r2
	loadw	(r2),r5
	addq	#2,r2
	loadw	(r2),r6
	addq	#2,r2
	loadw	(r2),r7
	addq	#2,r2
	loadw	(r2),r8
	addq	#2,r2
	loadw	(r2),r9
	addq	#2,r2
	loadw	(r2),r10
	addq	#2,r2
	loadw	(r2),r11
	movei	#$f00402,r2
	load	(r2),r12
	load	(r3),r13
	movei	#$f00410,r14
	store	r0,(r14)
	store	r0,(r14+4)
	store	r0,(r14+8)
	store	r0,(r14+12)
	movei	#$77778888,r1
	movei	#$f00416,r2
	store	r1,(r2)
	movei	#$9999aaaa,r1
	movei	#$10016,r15
	store	r1,(r15)
	loadw	(r14),r16
	addq	#2,r14
	loadw	(r14),r17
	addq	#2,r14
	loadw	(r14),r18
	addq	#2,r14
	loadw	(r14),r19
	addq	#2,r14
	loadw	(r14),r20
	addq	#2,r14
	loadw	(r14),r21
	load	(r2),r22
	load	(r15),r23
	movei	#G_CTRL,r30
	moveq	#0,r31
.stop	store	r31,(r30)
	jr	.stop
	nop
	END
