;-*-asm-*-
; RUN: --gpu --mem $10000:$100
; A taken JR to a target in the second word of a long word, a STORE (R14+Rn) to local RAM in its
; delay slot whose data the instruction before the JR's last writes (RORQ); the loop around it
; reads bytes from main RAM four at a time, as the BIOS checks a cartridge.
	gpu
	include "jaghard.inc"
	RUN $F03000
	movei	#$10000,r3
	movei	#$01234567,r4
	movei	#$89abcdef,r5
	store	r4,(r3)
	addq	#4,r3
	store	r5,(r3)
	movei	#$ffff,r0
	movei	#.buf,r14
	moveq	#8,r1
	nop
.outer	moveq	#0,r13
	moveq	#4,r2
.inner	addq	#1,r0
	loadb	(r0),r16
	or	r16,r13
	subq	#1,r2
	jr	ne,.inner
	rorq	#8,r13
	subq	#4,r1
	jr	ne,.outer
	store	r13,(r14+r1)
	load	(r14+4),r6
	movei	#$10020,r3
	store	r6,(r3)
	moveq	#1,r5
.done	addq	#1,r6
	jr	.done
	nop
	long
.buf	dc.l	0,0,0,0
	END
