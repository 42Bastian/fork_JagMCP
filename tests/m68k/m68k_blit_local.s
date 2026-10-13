| RUN: --m68k --cycles 12000 --mem $20000:$100
| The blitter at the RISCs' local spaces, 16 bits wide on Tom's and Jerry's buses: 16 pixels of 32
| bits copied a phrase at a time from main RAM into the GPU's RAM, into the DSP's RAM, and from the
| GPU's RAM back to main RAM; the 68000 reads the copies out through the slave ports.
	.text
	lea	0x20000,%a0		| the source
	move.l	#0x12345678,%d3
	moveq	#15,%d2
1:	move.l	%d3,(%a0)+
	add.l	#0x01030507,%d3
	dbra	%d2,1b
	lea	0xf02200,%a1
	move.l	#0xf03800,%d0		| into the GPU's RAM
	bsr	blit
	move.l	#0xf1c000,%d0		| into the DSP's RAM
	bsr	blit
	move.l	#0x20080,(%a1)		| A1_BASE: back to main RAM
	move.l	#0xf03800,0x24(%a1)	| A2_BASE: from the GPU's RAM
	bsr	go
	lea	0xf03800,%a2		| the copies read out
	lea	0x20040,%a3
	moveq	#7,%d2
2:	move.l	(%a2)+,(%a3)+
	dbra	%d2,2b
	lea	0xf1c000,%a2
	moveq	#7,%d2
3:	move.l	(%a2)+,(%a3)+
	dbra	%d2,3b
	lea	0x20080,%a2		| the copy back, read
	moveq	#15,%d2
5:	move.l	(%a2)+,%d1
	dbra	%d2,5b
	stop	#0x2700

| a copy from $20000 to d0
blit:	move.l	%d0,(%a1)		| A1_BASE
	move.l	#0x20000,0x24(%a1)	| A2_BASE
go:	move.l	#0x00002828,4(%a1)	| A1_FLAGS: PITCH1, PIXEL32, WID32, XADDPHR
	move.l	#0,0xc(%a1)		| A1_PIXEL
	move.l	#0x00002828,0x28(%a1)	| A2_FLAGS
	move.l	#0,0x30(%a1)		| A2_PIXEL
	move.l	#0x00010010,0x3c(%a1)	| B_COUNT: 1 line of 16
	move.l	#0x01800601,0x38(%a1)	| B_CMD: SRCEN, UPDA1, UPDA2, LFU_S
4:	move.l	0x38(%a1),%d1
	btst	#0,%d1
	beq.s	4b
	rts
