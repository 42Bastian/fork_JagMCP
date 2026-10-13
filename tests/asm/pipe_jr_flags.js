;-*-asm-*-
; conditional JR right after the flag setting instruction
	include "jaghard.inc"
	include "help.mac"

	dsp
	run	D_RAM

	moveq	#0,r0
	nop
	nop
	cmpq	#1,r0		; Z = 0
	jr	eq,.x		; waits for the flags
	nop
	moveq	#1,r1
.x	moveq	#2,r2
	moveq	#3,r3
	nop
	nop
	STOP_DSP
