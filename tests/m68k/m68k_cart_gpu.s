| RUN: --m68k --cycles 30000 --mem $30000:$100 --mem $40000:$200
| What the GPU and the blitter read of the cartridge: at each ROMWIDTH of MEMCON1 (8 to 32 bits,
| the slowest, and 32 bits the fastest with FASTROM) the 68000 starts a GPU program that reads it a
| byte, a word, a long word and a phrase at a time and stores what it read into main RAM, then
| waits for its flag there; then the blitter copies 32 pixels of 16 bits from it to main RAM a
| phrase at a time, 32 bits wide, and 8 pixels a pixel at a time, 8 bits wide.
	.text
	lea	gpucode,%a0
	lea	0xf03000,%a1
	moveq	#19,%d0
1:	move.l	(%a0)+,(%a1)+
	dbra	%d0,1b
	lea	table,%a3
	move.l	#0x30000,%d6
	moveq	#3,%d7
2:	move.w	(%a3)+,0xf00000
	move.l	%d6,0x30200		| where the GPU stores
	clr.l	0x30204			| its flag
	move.l	#0xf03000,0xf02110	| G_PC
	move.l	#1,0xf02114		| G_CTRL: GO
3:	tst.l	0x30204
	beq.s	3b
	add.l	#0x20,%d6
	dbra	%d7,2b

	move.w	#0x1865,0xf00000	| 32 bits
	lea	0xf02200,%a1		| A1_BASE
	move.l	#0x40000,(%a1)
	move.l	#0x00002820,4(%a1)	| A1_FLAGS: PITCH1, PIXEL16, WID32, XADDPHR
	move.l	#0,0xc(%a1)		| A1_PIXEL
	move.l	#0x800000,0x24(%a1)	| A2_BASE
	move.l	#0x00002820,0x28(%a1)	| A2_FLAGS
	move.l	#0,0x30(%a1)		| A2_PIXEL
	move.l	#0x00010020,0x3c(%a1)	| B_COUNT: 1 line of 32
	move.l	#0x01800601,0x38(%a1)	| B_CMD: SRCEN, UPDA1, UPDA2, LFU_S
4:	move.l	0x38(%a1),%d0
	btst	#0,%d0
	beq.s	4b
	move.w	#0x1861,0xf00000	| 8 bits
	move.l	#0x40100,(%a1)		| A1_BASE
	move.l	#0x00011820,4(%a1)	| A1_FLAGS: PITCH1, PIXEL16, WID8, XADDPIX
	move.l	#0,0xc(%a1)		| A1_PIXEL
	move.l	#0x800010,0x24(%a1)	| A2_BASE
	move.l	#0x00011820,0x28(%a1)	| A2_FLAGS
	move.l	#0,0x30(%a1)		| A2_PIXEL
	move.l	#0x00010008,0x3c(%a1)	| B_COUNT: 1 line of 8
	move.l	#0x01800601,0x38(%a1)	| B_CMD
5:	move.l	0x38(%a1),%d0
	btst	#0,%d0
	beq.s	5b
	stop	#0x2700

table:	.word	0x1861,0x1863,0x1865,0x18fd

	.even
| movei #$30200,r14; load (r14),r2; movei #$800010,r1; loadb (r1),r3; store r3,(r2); addq #4,r2;
| addq #1,r1; loadb (r1),r3; store r3,(r2); addq #4,r2; addq #1,r1; loadw (r1),r3; store r3,(r2);
| addq #4,r2; subq #2,r1; load (r1),r3; store r3,(r2); addq #4,r2; loadp (r1),r3;
| movei #G_HIDATA,r4; load (r4),r5; store r5,(r2); addq #4,r2; store r3,(r2); moveq #1,r6;
| store r6,(r14+4); movei #G_CTRL,r4; moveq #0,r5; store r5,(r4); nop; nop (lyxass)
gpucode:
	.word	0x980e, 0x0200, 0x0003, 0xa5c2, 0x9801, 0x0010, 0x0080, 0x9c23, 0xbc43, 0x0882
	.word	0x0821, 0x9c23, 0xbc43, 0x0882, 0x0821, 0xa023, 0xbc43, 0x0882, 0x1841, 0xa423
	.word	0xbc43, 0x0882, 0xa823, 0x9804, 0x2118, 0x00f0, 0xa485, 0xbc45, 0x0882, 0xbc43
	.word	0x8c26, 0xc426, 0x9804, 0x2114, 0x00f0, 0x8c05, 0xbc85, 0xe400, 0xe400, 0x0000

	.section .cart,"ax"
	.long	0x04040404,0x10213243,0x54657687,0x98a9bacb
	.byte	0xa0,0xa1,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,0xa8,0xa9,0xaa,0xab,0xac,0xad,0xae,0xaf
	.long	0x01234567,0x89abcdef,0x02468ace,0x13579bdf,0xfedcba98,0x76543210,0x0f1e2d3c,0x4b5a6978
	.long	0x11223344,0x55667788,0x99aabbcc,0xddeeff00
