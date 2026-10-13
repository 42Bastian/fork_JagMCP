;-*-asm-*-
; RUN: --gpu --mem $40000:$400
; The blitter copies 64 by 4 pixels of 16 bits a phrase at a time, from A2 at $20000 to A1 at
; $40000 (the source read, the destination written, LFU_S), the pointers stepped to the next line
; at the end of each; the GPU fills the source first and waits on B_CMD's idle bit after.
	gpu
	include "jaghard.inc"
	RUN $F03000
	movei	#$20000,r14
	movei	#$12345678,r3
	movei	#$01030507,r4
	movei	#128,r2
.fill	store	r3,(r14)
	addq	#4,r14
	add	r4,r3
	subq	#1,r2
	jr	ne,.fill
	nop
	movei	#A1_BASE,r14
	movei	#$40000,r0
	store	r0,(r14)			; A1_BASE
	movei	#PITCH1|PIXEL16|WID64|XADDPHR,r0
	store	r0,(r14+4)			; A1_FLAGS
	moveq	#0,r0
	store	r0,(r14+$c)			; A1_PIXEL
	movei	#$0001ffc0,r0
	store	r0,(r14+$10)			; A1_STEP: y + 1, x - 64
	movei	#$20000,r0
	store	r0,(r14+$24)			; A2_BASE
	movei	#PITCH1|PIXEL16|WID64|XADDPHR,r0
	store	r0,(r14+$28)			; A2_FLAGS
	moveq	#0,r0
	store	r0,(r14+$30)			; A2_PIXEL
	movei	#$0001ffc0,r0
	store	r0,(r14+$34)			; A2_STEP
	movei	#$00040040,r0
	store	r0,(r14+$3c)			; B_COUNT: 4 lines of 64
	movei	#SRCEN|UPDA1|UPDA2|LFU_S,r0
	store	r0,(r14+$38)			; B_CMD
.wait	load	(r14+$38),r1
	btst	#0,r1
	jr	eq,.wait
	nop
	moveq	#1,r5
.done	addq	#1,r6
	jr	.done
	nop
	END
