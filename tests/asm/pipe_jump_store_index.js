;-*-asm-*-
; RUN: --gpu --mem $10000:$100
; JUMPs to targets outside the queue with a STORE (R14+n) or (R15+n) in their delay slot, to local
; RAM, to main RAM and to the blitter's registers. The queue is empty as the store takes its
; second cycle, so the instruction register takes a stale word (ins_exec.v mwordi with romold),
; not the store again, whose data register would hold the next instruction back (yarc reloaded).
	gpu
	include "jaghard.inc"
	RUN $F03000
	movei	#.buf,r14
	movei	#$10000,r15
	movei	#$f02200,r13
	moveq	#0,r1
	moveq	#6,r10
	movei	#.t1,r5
	movei	#.t2,r6
	movei	#.t3,r7
	movei	#.t4,r8
	movei	#.loop,r9
.loop	jump	(r5)
	store	r1,(r14+4)
	nop
	nop
	nop
	nop
	nop
	nop
.t1	movei	#$12345678,r2
	addq	#1,r1
	jump	(r6)
	store	r1,(r15+8)
	nop
	nop
	nop
	nop
	nop
	nop
.t2	add	r1,r2
	move	r15,r12
	move	r13,r15
	jump	(r7)
	store	r2,(r15+4)
	nop
	nop
	nop
	nop
	nop
	nop
.t3	moveq	#3,r3
	move	r12,r15
	jump	(r8)
	store	r3,(r14+8)
	nop
	nop
	nop
	nop
	nop
	nop
.t4	addq	#2,r1
	subq	#1,r10
	jump	ne,(r9)
	nop
	load	(r14+4),r4
	load	(r15+8),r6
	store	r4,(r15+16)
	store	r6,(r15+20)
.done	addq	#1,r6
	jr	.done
	nop
	long
.buf	dc.l	0,0,0,0
	END
