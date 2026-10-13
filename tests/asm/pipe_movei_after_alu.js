;-*-asm-*-
; MOVEI right after an ALU instruction
	include "jaghard.inc"
	include "help.mac"

	dsp
	run	D_RAM

	moveq	#1,r0
	nop
	nop
	addq	#1,r0
	movei	#$12345678,r1
	moveq	#3,r2
	nop
	nop
	STOP_DSP
