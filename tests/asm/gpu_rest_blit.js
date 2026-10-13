;-*-asm-*-
; RUN: --gpu --mem $20000:$100 --cycles 20000
; Video on, the object processor's list a stop and its interrupt enabled: the GPU is stepped with
; the bus in every cycle, and the bus rests between lines. The GPU's writes of the blitter's
; registers fall in cycles of a rest and land in them (yarc reloaded): three fills of a line of
; main RAM with the pattern, the GPU polling B_CMD until each is done.
	gpu
	include "jaghard.inc"
	RUN $F03000
	movei	#$10000,r14	; the list: a stop
	moveq	#0,r0
	moveq	#4,r2
	store	r0,(r14)
	store	r2,(r14+4)
	movei	#.regs,r1
	moveq	#17,r2
.set	load	(r1),r3		; address in the high half, value in the low
	addq	#4,r1
	move	r3,r4
	shrq	#16,r4
	bset	#23,r4
	bset	#22,r4
	bset	#21,r4
	bset	#20,r4
	storew	r3,(r4)
	subq	#1,r2
	jr	ne,.set
	nop
	movei	#G_FLAGS,r1
	movei	#$4080,r2	; REGPAGE, the object processor's interrupt
	store	r2,(r1)
	movei	#$f02200,r14
	movei	#$20000,r10
	moveq	#3,r11
	movei	#$12345678,r12
	movei	#.blit,r8
.blit	movei	#300,r0
.wait	subq	#1,r0
	jr	ne,.wait
	nop
	store	r10,(r14)	; A1_BASE
	movei	#$2828,r1
	store	r1,(r14+4)	; A1_FLAGS: PITCH1, PIXEL32, WID32, XADDPHR
	moveq	#0,r1
	store	r1,(r14+12)	; A1_PIXEL
	movei	#$00010010,r1
	store	r1,(r14+60)	; B_COUNT: 1 line of 16
	store	r12,(r14+104)	; B_PATD
	store	r12,(r14+108)
	movei	#$10000,r1
	store	r1,(r14+56)	; B_CMD: PATDSEL
.poll	load	(r14+56),r2
	btst	#0,r2
	jr	eq,.poll
	nop
	add	r11,r12
	movei	#64,r1
	add	r1,r10
	subq	#1,r11
	jump	ne,(r8)
	nop
.done	addq	#1,r9
	jr	.done
	nop
	long
.regs	dc.w	$0020,$0000	; OLP low
	dc.w	$0022,$0001	; OLP high
	dc.w	$002e,99	; HP
	dc.w	$0030,$400|80	; HBB
	dc.w	$0032,10	; HBE
	dc.w	$0034,$400|90	; HS
	dc.w	$0038,20	; HDB1
	dc.w	$003a,20	; HDB2
	dc.w	$003c,$400|70	; HDE
	dc.w	$003e,19	; VP
	dc.w	$0040,17	; VBB
	dc.w	$0042,2		; VBE
	dc.w	$0044,18	; VS
	dc.w	$0046,4		; VDB
	dc.w	$0048,14	; VDE
	dc.w	$002a,$1234	; BORD1
	dc.w	$0028,$6C1	; VMODE
	END
