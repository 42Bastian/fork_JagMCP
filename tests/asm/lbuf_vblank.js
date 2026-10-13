;-*-asm-*-
; RUN: --gpu
; The GPU's writes into the line buffers while no line is displayed: in every field, during the
; vertical blank, line buffers A and B are filled with long words of a sequence, words at their odd
; halves among them; the object processor runs a stop; the first displayed lines show them.
	gpu
	include "jaghard.inc"
	RUN $F03000
	movei	#$10000,r14	; the list: a stop
	moveq	#0,r2
	store	r2,(r14)
	moveq	#4,r2
	store	r2,(r14+4)
	movei	#.regs,r1
	moveq	#17,r2
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
	movei	#$12345678,r6
	movei	#$01030507,r7
	movei	#VC,r20
	movei	#$7ff,r21
	moveq	#17,r22
	movei	#.frame,r13
.frame	loadw	(r20),r0	; wait for the vertical blank: VC 17
	and	r21,r0
	cmp	r22,r0
	jr	ne,.frame
	nop
	movei	#$F00800,r5	; A and B, the long words along the line
	movei	#$F01000,r10
	moveq	#24,r8
.line	store	r6,(r5)
	add	r7,r6
	store	r6,(r10)
	addq	#4,r5
	addq	#4,r10
	storew	r7,(r10)	; a word at a high half
	subq	#1,r8
	jr	ne,.line
	addq	#2,r6
.out	loadw	(r20),r0	; wait for it to end
	and	r21,r0
	cmp	r22,r0
	jr	eq,.out
	nop
	jump	(r13)
	nop
	long
.regs	dc.w	$0020,$0000	; OLP low
	dc.w	$0022,$0001	; OLP high
	dc.w	$002e,99	; HP
	dc.w	$0030,$400|80	; HBB
	dc.w	$0032,30	; HBE
	dc.w	$0034,$400|90	; HS
	dc.w	$0038,20	; HDB1
	dc.w	$003a,20	; HDB2
	dc.w	$003c,$400|70	; HDE
	dc.w	$003e,19	; VP
	dc.w	$0040,17	; VBB
	dc.w	$0042,2		; VBE
	dc.w	$0044,18	; VS
	dc.w	$0046,4		; VDB
	dc.w	$0048,14	; VDE
	dc.w	$002a,$1234	; BORD1
	dc.w	$0028,$6C7	; VMODE: RGB16, background write
	END
