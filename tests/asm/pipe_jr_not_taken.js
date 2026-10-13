;-*-asm-*-
; conditional JR not taken, flags settled long before
	include "jaghard.inc"
	include "help.mac"

	dsp
	run	D_RAM

	moveq	#0,r0
	cmpq	#1,r0		; Z = 0
	nop
	nop
	nop
	jr	eq,.x		; not taken
	nop
	moveq	#1,r1
.x	moveq	#2,r2
	moveq	#3,r3
	nop
	nop
	STOP_DSP
