;-*-asm-*-
; RUN: --gpu --cycles 40000
; The video time base of video_border; after some frames VBB is moved past VP (Native, a
; prototype, sets it so): the vertical blank no longer begins, and each frame ends as VC wraps.
; The border colour changes every 20 cycles or so, as there.
	gpu
	include "jaghard.inc"
	RUN $F03000
	movei	#$F00000,r14
	movei	#.regs,r1
	moveq	#14,r2
.set	load	(r1),r3		; address in the high half, value in the low
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
	movei	#BORD1,r5
	movei	#$1234,r6
	moveq	#0,r7
	movei	#300,r9
	movei	#VBB,r11
	moveq	#25,r10
.loop	addq	#7,r6
	storew	r6,(r5)
	addq	#1,r7
	cmp	r7,r9
	jr	ne,.wait0
	nop
	storew	r10,(r11)	; VBB past VP from here on
.wait0	moveq	#13,r8
.wait	subq	#1,r8
	jr	ne,.wait
	nop
	jr	.loop
	nop
	long
.regs	dc.w	$002e,99	; HP
	dc.w	$0030,$400|80	; HBB
	dc.w	$0032,10	; HBE
	dc.w	$0034,$400|90	; HS
	dc.w	$0038,20	; HDB1
	dc.w	$003a,20	; HDB2
	dc.w	$003c,$400|70	; HDE
	dc.w	$003e,19	; VP
	dc.w	$0040,17	; VBB
	dc.w	$0042,2		; VBE
	dc.w	$0044,18	; VS
	dc.w	$0046,$7ff	; VDB
	dc.w	$0048,$7ff	; VDE
	dc.w	$0028,$6C1	; VMODE
	END
