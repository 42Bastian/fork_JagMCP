;-*-asm-*-
; ALU write-back and an immediate write (moveq) in the same cycle
	include "jaghard.inc"
	include "help.mac"

	dsp
	run	D_RAM

	moveq	#1,r0
	nop
	nop
	addq	#1,r0		; ALU result written two cycles later
	moveq	#5,r1		; reads no register: its port is free
	moveq	#6,r2
	nop
	nop
	STOP_DSP
