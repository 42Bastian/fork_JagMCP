;-*-asm-*-
; RUN: --gpu --mem $30000:$30
; The blitter writes the line buffers, no line displayed: eight long words of 32 bit pixels
; through Tom's 32 bit window onto buffer A ($F08800, both pixels of a long word at once), then
; eight 16 bit pixels into buffer B ($F01000, a word each I/O cycle); the GPU reads both back to
; $30000.
	gpu
	include "jaghard.inc"
	RUN $F03000
	movei	#$20000,r14	; the source: sixteen long words of a sequence
	movei	#$12345678,r3
	movei	#$01030507,r4
	moveq	#16,r2
.fill	store	r3,(r14)
	addq	#4,r14
	add	r4,r3
	subq	#1,r2
	jr	ne,.fill
	nop
	movei	#A1_BASE,r14
	movei	#$F08800,r0
	store	r0,(r14)			; A1_BASE: buffer A, 32 bits wide
	movei	#PITCH1|PIXEL32|WID64|XADDPIX,r0
	store	r0,(r14+4)			; A1_FLAGS
	store	r0,(r14+$28)			; A2_FLAGS
	moveq	#0,r0
	store	r0,(r14+$c)			; A1_PIXEL
	store	r0,(r14+$30)			; A2_PIXEL
	movei	#$20000,r0
	store	r0,(r14+$24)			; A2_BASE
	movei	#$00010008,r0
	store	r0,(r14+$3c)			; B_COUNT: a line of 8
	movei	#SRCEN|LFU_S,r0
	store	r0,(r14+$38)			; B_CMD
.wait1	load	(r14+$38),r1
	btst	#0,r1
	jr	eq,.wait1
	nop
	movei	#$F01000,r0
	store	r0,(r14)			; A1_BASE: buffer B, 16 bits wide
	movei	#PITCH1|PIXEL16|WID64|XADDPIX,r0
	store	r0,(r14+4)			; A1_FLAGS
	store	r0,(r14+$28)			; A2_FLAGS
	moveq	#0,r0
	store	r0,(r14+$c)			; A1_PIXEL
	store	r0,(r14+$30)			; A2_PIXEL
	movei	#$20020,r0
	store	r0,(r14+$24)			; A2_BASE
	movei	#$00010008,r0
	store	r0,(r14+$3c)			; B_COUNT: a line of 8
	movei	#SRCEN|LFU_S,r0
	store	r0,(r14+$38)			; B_CMD
.wait2	load	(r14+$38),r1
	btst	#0,r1
	jr	eq,.wait2
	nop
	movei	#$F00800,r1	; A's eight long words, then B's four, to $30000
	movei	#$30000,r2
	moveq	#8,r5
.readA	load	(r1),r3
	addq	#4,r1
	store	r3,(r2)
	subq	#1,r5
	jr	ne,.readA
	addq	#4,r2
	movei	#$F01000,r1
	moveq	#4,r5
.readB	load	(r1),r3
	addq	#4,r1
	store	r3,(r2)
	subq	#1,r5
	jr	ne,.readB
	addq	#4,r2
	moveq	#1,r5
.done	addq	#1,r6
	jr	.done
	nop
	END
