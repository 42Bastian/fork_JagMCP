;-*-asm-*-
; unconditional JR taken
	include "jaghard.inc"
	include "help.mac"

	dsp
	run	D_RAM

	moveq	#0,r0
	nop
	nop
	jr	.x
	nop
	moveq	#1,r1
.x	moveq	#2,r2
	moveq	#3,r3
	nop
	nop
	STOP_DSP
