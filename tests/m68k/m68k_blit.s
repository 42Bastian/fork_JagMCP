| RUN: --m68k --cycles 9000 --mem $20000:$400 --mem $40000:$400
| The 68000 sets the blitter going itself, its registers written through the GPU's slave port: 64
| by 4 pixels of 16 bits copied a phrase at a time from A2 at $20000 to A1 at $40000, as blit_copy
| has the GPU do it; it polls B_CMD's idle bit, read back through the port, while the blitter has
| the bus.
	.text
	lea	0x20000,%a0		| the source
	move.l	#0x12345678,%d3
	move.w	#127,%d2
1:	move.l	%d3,(%a0)+
	add.l	#0x01030507,%d3
	dbra	%d2,1b
	lea	0xf02200,%a1		| A1_BASE
	move.l	#0x40000,(%a1)
	move.l	#0x00003020,4(%a1)	| A1_FLAGS: PITCH1, PIXEL16, WID64, XADDPHR
	move.l	#0,0xc(%a1)		| A1_PIXEL
	move.l	#0x0001ffc0,0x10(%a1)	| A1_STEP: y + 1, x - 64
	move.l	#0x20000,0x24(%a1)	| A2_BASE
	move.l	#0x00003020,0x28(%a1)	| A2_FLAGS
	move.l	#0,0x30(%a1)		| A2_PIXEL
	move.l	#0x0001ffc0,0x34(%a1)	| A2_STEP
	move.l	#0x00040040,0x3c(%a1)	| B_COUNT: 4 lines of 64
	move.l	#0x01800601,0x38(%a1)	| B_CMD: SRCEN, UPDA1, UPDA2, LFU_S
2:	move.l	0x38(%a1),%d0
	btst	#0,%d0
	beq.s	2b
	move.l	%d0,0x40400
	stop	#0x2700
