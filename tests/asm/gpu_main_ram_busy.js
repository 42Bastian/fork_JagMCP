;-*-asm-*-
; RUN: --gpu --mem $10000:$100 --mem $20000:$800
; The GPU runs from main RAM while the blitter, at high priority (BUSHI), fills it: a fetch waits
; for the bus, a load of the code queues behind it in the gateway, and the two cycles run back to
; back. The fetch's long word is taken as the load's cycle starts (prog_data), not the load's data
; (yarc reloaded). Then a routine copied into the local RAM stops the GPU.
	gpu
	include "jaghard.inc"
	RUN $4000
	movei	#$f02200,r14
	movei	#$20000,r1
	store	r1,(r14)	; A1_BASE
	movei	#$2828,r1
	store	r1,(r14+4)	; A1_FLAGS: PITCH1, PIXEL32, WID32, XADDPHR
	moveq	#0,r1
	store	r1,(r14+12)	; A1_PIXEL
	movei	#$00010200,r1
	store	r1,(r14+60)	; B_COUNT: 1 line of 512
	movei	#$55aa33cc,r1
	store	r1,(r14+104)	; B_PATD
	store	r1,(r14+108)
	movei	#$10000,r15
	moveq	#0,r2
	movei	#BUSHI|PATDSEL,r1
	store	r1,(r14+56)	; B_CMD
	REPT	24
	load	(r15),r3
	add	r3,r2
	addq	#4,r15
	ENDR
.poll	load	(r14+56),r3
	btst	#0,r3
	jr	eq,.poll
	nop
	movei	#$10080,r15
	store	r2,(r15)
	movei	#local,r0
	movei	#$f03000,r6
	movei	#(local_e-local)/4,r7
.copy	load	(r0),r8
	addq	#4,r0
	store	r8,(r6)
	subq	#1,r7
	jr	ne,.copy
	addq	#4,r6
	movei	#$f03000,r0
	jump	(r0)
	nop
	long
local	movei	#G_CTRL,r3
	moveq	#0,r4
	store	r4,(r3)
	nop
	nop
	nop
local_e
	END
