;-*-asm-*-
; RUN: --gpu
; The object processor's GPU object: a list of a GPU object and a stop at $10000, the GPU taking
; its interrupt (3) in every displayed line, its routine writing OBF so that the object processor
; goes on, counting them, while the main loop reads main RAM.
	gpu
	include "jaghard.inc"
	RUN $F03000
	movei	#init,r0
	jump	(r0)
	nop
	align	16
	REPT	16
	nop
	ENDR
; the object processor's interrupt (3)
	movei	#G_FLAGS,r30
	load	(r30),r29
	bset	#12,r29		; clear the latch
	bclr	#3,r29		; and IMASK
	movei	#OBF,r28
	storew	r28,(r28)	; the object processor goes on
	addq	#1,r26
	load	(r31),r28
	addq	#2,r28
	addq	#4,r31
	jump	(r28)
	store	r29,(r30)
	align	4
init:
	movei	#$F03F00,r31
	moveta	r31,r31
	moveq	#0,r0
	moveta	r0,r26
	movei	#$10000,r14	; the list: a GPU object, a stop
	moveq	#2,r2
	store	r0,(r14)
	store	r2,(r14+4)
	moveq	#4,r2
	store	r0,(r14+8)
	store	r2,(r14+12)
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
	movei	#G_FLAGS,r1
	movei	#$4080,r2	; REGPAGE, the object processor's interrupt
	store	r2,(r1)
	movei	#$1000,r5
.loop	load	(r5),r7
	addq	#1,r9
	jr	.loop
	nop
	long
.regs	dc.w	$0020,$0000	; OLP low
	dc.w	$0022,$0001	; OLP high
	dc.w	$002e,99	; HP
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
	dc.w	$0046,4		; VDB
	dc.w	$0048,14	; VDE
	dc.w	$002a,$1234	; BORD1
	dc.w	$0028,$6C1	; VMODE
	END
