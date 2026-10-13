;-*-asm-*-
; unconditional JUMP taken
	include "jaghard.inc"
	include "help.mac"

	dsp
	run	D_RAM

	movei	#.x,r4
	nop
	nop
	jump	(r4)
	nop
	moveq	#1,r1
.x	moveq	#2,r2
	moveq	#3,r3
	nop
	nop
	STOP_DSP
