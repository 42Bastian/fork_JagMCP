;-*-asm-*-
; IMULTN / IMACN / RESMAC sequence
	include "jaghard.inc"
	include "help.mac"

	dsp
	run	D_RAM

	moveq	#2,r1
	moveq	#3,r2
	moveq	#4,r3
	moveq	#5,r4
	nop
	nop
	imultn	r1,r2		; 6
	imacn	r3,r4		; + 20
	resmac	r0
	moveq	#7,r5
	nop
	nop
	STOP_DSP
