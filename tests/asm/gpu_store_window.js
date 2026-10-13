;-*-asm-*-
; RUN: --gpu
; The GPU writes its own local RAM through Tom's 32 bit window onto it ($F0B000, the local RAM
; $8000 above): STOREP phrases and STORE long words, as Ruiner Pinball loads its code modules,
; then reads them back at $F03xxx.
	gpu
	include "jaghard.inc"
	RUN $F03000
	movei	#$f03400,r14
	moveq	#0,r0
	moveq	#8,r1
.clear	store	r0,(r14)
	subq	#1,r1
	jr	ne,.clear
	addq	#4,r14
	movei	#$11223344,r2
	movei	#$55667788,r3
	movei	#G_HIDATA,r4
	store	r2,(r4)			; the phrase's high long word
	movei	#$f0b400,r5
	storep	r3,(r5)			; a phrase at $F0B400: $F03400 and $F03404
	movei	#$99aabbcc,r6
	movei	#$f0b410,r7
	store	r6,(r7)			; a long word at $F0B410: $F03410
	movei	#$f03400,r8
	load	(r8),r9
	addq	#4,r8
	load	(r8),r10
	movei	#$f03410,r8
	load	(r8),r11
	movei	#G_CTRL,r30
	moveq	#0,r31
.stop	store	r31,(r30)
	jr	.stop
	nop
	END
