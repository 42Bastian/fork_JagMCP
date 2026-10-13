;-*-asm-*-
; DIV latency: the quotient is read by the next instruction
	include "jaghard.inc"
	include "help.mac"

	dsp
	run	D_RAM

	movei	#$123,r1
	movei	#$12345678,r2
	nop
	nop
	div	r1,r2
	move	r2,r3		; waits for the quotient
	nop
	nop
	STOP_DSP
