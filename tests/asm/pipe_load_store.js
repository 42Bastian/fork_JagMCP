;-*-asm-*-
; local RAM loads and stores back to back
	include "jaghard.inc"
	include "help.mac"

	dsp
	run	D_RAM

	movei	#D_RAM+$200,r14
	moveq	#$11,r1
	moveq	#$22-$20,r2
	addq	#$20,r2
	nop
	store	r1,(r14)
	store	r2,(r14+4)
	load	(r14),r3
	load	(r14+4),r2
	moveq	#4,r4
	nop
	nop
	STOP_DSP
