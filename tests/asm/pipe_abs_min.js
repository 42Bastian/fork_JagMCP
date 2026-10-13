;-*-asm-*-
; ABS of $80000000 followed by a read of the result
	include "jaghard.inc"
	include "help.mac"

	dsp
	run	D_RAM

; RUN: --cycles 2000
	movei	#$80000000,r8
	nop
	nop
	abs	r8
	move	r8,r9		; reads the ABS result
	moveq	#3,r3
	nop
	nop
	STOP_DSP
