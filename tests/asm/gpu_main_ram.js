;-*-asm-*-
; RUN: --gpu --mem $10000:$100
; The GPU runs its program from main RAM: each fetch a long word through the gateway, as the BIOS
; leaves a cartridge's GPU code to (yarc reloaded). The loop interleaves the fetches with stores to
; main RAM and to Tom's registers (the CLUT, a long word and words, 16 bits wide), loads, a JUMP
; and its delay slot; then it copies a routine into its local RAM, jumps there and stops.
	gpu
	include "jaghard.inc"
	RUN $4000
	movei	#$10000,r14
	movei	#$f00400,r5
	moveq	#0,r1
	movei	#20,r2
	movei	#.loop,r9
.loop	store	r1,(r14)
	addq	#4,r14
	store	r1,(r5)
	addq	#4,r5
	storew	r1,(r5)
	addq	#2,r5
	move	r1,r4
	neg	r4
	storew	r4,(r5)
	addq	#2,r5
	shrq	#16,r4
	storew	r4,(r5)
	addq	#2,r5
	load	(r14),r4
	add	r4,r1
	addq	#3,r1
	subq	#1,r2
	jump	ne,(r9)
	or	r1,r4
	movei	#$10080,r14
	store	r1,(r14)
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
