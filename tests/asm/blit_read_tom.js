;-*-asm-*-
; RUN: --gpu --mem $30000:$40
; The blitter reads Tom's own space: the GPU writes eight CLUT entries ($F00410) and eight 16 bit
; pixels of line buffer B ($F01000), no line displayed; the blitter copies the CLUT's to $30000 in
; pixel mode and again to $30010 in phrase mode, and the line buffer's to $30020 in pixel mode.
	gpu
	include "jaghard.inc"

; copies eight 16 bit pixels from \0 to \1, flags \2
	macro BLIT
	movei	#\1,r0
	store	r0,(r14)			; A1_BASE: the destination
	movei	#\2,r0
	store	r0,(r14+4)			; A1_FLAGS
	store	r0,(r14+$28)			; A2_FLAGS
	movei	#\0,r0
	store	r0,(r14+$24)			; A2_BASE: the source
	moveq	#0,r0
	store	r0,(r14+$c)			; A1_PIXEL
	store	r0,(r14+$30)			; A2_PIXEL
	movei	#$00010008,r0
	store	r0,(r14+$3c)			; B_COUNT: a line of 8
	movei	#SRCEN|LFU_S,r0
	store	r0,(r14+$38)			; B_CMD
.\w	load	(r14+$38),r1
	btst	#0,r1
	jr	eq,.\w
	nop
	endm

	RUN $F03000
	movei	#CLUT+$10,r1	; eight words of a sequence into each
	movei	#$F01000,r2
	movei	#$12345678,r3
	movei	#$01030507,r4
	moveq	#4,r5
.fill	store	r3,(r1)
	addq	#4,r1
	not	r3
	store	r3,(r2)
	not	r3
	addq	#4,r2
	subq	#1,r5
	jr	ne,.fill
	add	r4,r3
	movei	#A1_BASE,r14
	BLIT	CLUT+$10,$30000,PITCH1|PIXEL16|WID64|XADDPIX	; the CLUT, pixel mode
	BLIT	CLUT+$10,$30010,PITCH1|PIXEL16|WID64|XADDPHR	; the CLUT, phrase mode
	BLIT	$F01000,$30020,PITCH1|PIXEL16|WID64|XADDPIX	; line buffer B, pixel mode
.done	addq	#1,r6
	jr	.done
	nop
	END
