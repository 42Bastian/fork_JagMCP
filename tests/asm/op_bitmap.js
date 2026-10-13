;-*-asm-*-
; RUN: --gpu
; The object processor: a list of a 16 bit CRY bitmap and a stop at $10000, the pixels at $20000,
; a display window of ten half lines. The bitmap is drawn while its height lasts (the header is
; written back), meanwhile the GPU reads main RAM and HC.
	gpu
	include "jaghard.inc"
	RUN $F03000
	movei	#$10000,r14
	movei	#.list,r1
	moveq	#8,r2
.cl	load	(r1),r3
	addq	#4,r1
	store	r3,(r14)
	addq	#4,r14
	subq	#1,r2
	jr	ne,.cl
	nop
	movei	#$20000,r14
	movei	#$1234,r3
	movei	#160,r2
.px	storew	r3,(r14)
	addq	#2,r14
	movei	#$0711,r4
	add	r4,r3
	subq	#1,r2
	jr	ne,.px
	nop
	movei	#.regs,r1
	moveq	#17,r2
.set	load	(r1),r3
	addq	#4,r1
	move	r3,r4
	shrq	#16,r4
	bset	#23,r4
	bset	#22,r4
	bset	#21,r4
	bset	#20,r4
	storew	r3,(r4)
	subq	#1,r2
	jr	ne,.set
	nop
	movei	#$1000,r5
	movei	#HC,r6
.loop	load	(r5),r7
	loadw	(r6),r8
	addq	#1,r9
	jr	.loop
	nop
	long
.list
	dc.l	$02000020,$02014030
	dc.l	$00000000,$8020c004
	dc.l	$00000000,$00000004
	dc.l	$00000000,$00000000
.regs
	dc.w	$0020,$0000
	dc.w	$0022,$0001
	dc.w	$002e,$0063
	dc.w	$0030,$0450
	dc.w	$0032,$000a
	dc.w	$0034,$045a
	dc.w	$0038,$0014
	dc.w	$003a,$0014
	dc.w	$003c,$0446
	dc.w	$003e,$0013
	dc.w	$0040,$0011
	dc.w	$0042,$0002
	dc.w	$0044,$0012
	dc.w	$0046,$0004
	dc.w	$0048,$000e
	dc.w	$002a,$1234
	dc.w	$0028,$06c1
	END
