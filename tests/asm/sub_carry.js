;-*-asm-*-
; C after SUB, SUBQ, CMPQ and SUBQMOD is the borrow out of Rd - Rs (SoftRef), one bit of r0 per case
	include "jaghard.inc"
	include "help.mac"

	dsp
	run	D_RAM

	moveq	#0,r0

	moveq	#1,r1
	moveq	#2,r2
	sub	r2,r1		; 1 - 2: borrow
	jr	cc,.a
	nop
	bset	#0,r0
.a
	moveq	#2,r1
	moveq	#1,r2
	sub	r2,r1		; 2 - 1: no borrow
	jr	cs,.b
	nop
	bset	#1,r0
.b
	moveq	#1,r1
	subq	#2,r1		; 1 - 2: borrow
	jr	cc,.c
	nop
	bset	#2,r0
.c
	moveq	#3,r1
	subq	#2,r1		; 3 - 2: no borrow
	jr	cs,.d
	nop
	bset	#3,r0
.d
	moveq	#1,r1
	cmpq	#2,r1		; 1 - 2: borrow
	jr	cc,.e
	nop
	bset	#4,r0
.e
	moveq	#3,r1
	cmpq	#2,r1		; 3 - 2: no borrow
	jr	cs,.f
	nop
	bset	#5,r0
.f
	moveq	#0,r1
	cmpq	#-1,r1		; 0 - $ffffffff: borrow
	jr	cc,.g
	nop
	bset	#6,r0
.g
	movei	#D_MOD,r3
	moveq	#0,r4
	store	r4,(r3)		; no modulo bits: plain subtraction
	moveq	#1,r1
	subqmod	#2,r1		; 1 - 2: borrow
	jr	cc,.h
	nop
	bset	#7,r0
.h
	moveq	#3,r1
	subqmod	#2,r1		; 3 - 2: no borrow
	jr	cs,.i
	nop
	bset	#8,r0
.i
	STOP_DSP
