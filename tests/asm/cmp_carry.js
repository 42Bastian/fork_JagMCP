;-*-asm-*-
; CMP sets C to the borrow out of Rd - Rs (SoftRef: "C - represents borrow out of the subtract")
	include "jaghard.inc"
	include "help.mac"

	dsp
	run	D_RAM

	moveq	#0,r0
	moveq	#1,r1
	moveq	#2,r2

	cmp	r2,r1		; 1 - 2: borrow, C = 1
	jr	cc,.nc
	nop
	addq	#1,r0
.nc
	cmp	r1,r2		; 2 - 1: no borrow, C = 0
	jr	cs,.c
	nop
	addq	#2,r0
.c
	STOP_DSP
