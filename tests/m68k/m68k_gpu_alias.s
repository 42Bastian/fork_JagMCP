| RUN: --m68k --cycles 30000 --mem $30000:$40
| The GPU's space at its mirrors (abus.v gpuadd: Tom's space with address bit 13 or 14) and its
| 32 bit writes from $F08000 (gpu_cpu.v at[15]: each half writes the long word of the data bus's
| lanes): the 68000's long words and a word written at $F0B000, read back at $F03000 and at the
| mirror $F05000; then the blitter copies a GPU program to $F0B100, as yarc's start does, and the
| GPU runs it from $F03100, storing a marker in main RAM.
	.text
	move.l	#0x12345678,0xf0b000
	move.l	#0x9abcdef0,0xf0b004
	move.w	#0x5555,0xf0b00a
	lea	0x30000,%a0
	move.l	0xf03000,(%a0)+
	move.l	0xf03004,(%a0)+
	move.l	0xf03008,(%a0)+
	move.l	0xf05000,(%a0)+		| the mirror
	lea	gpucode,%a1		| the program, to main RAM
	lea	0x20000,%a2
	moveq	#7,%d0
1:	move.l	(%a1)+,(%a2)+
	dbra	%d0,1b
	lea	0xf02200,%a1
	move.l	#0xf0b100,(%a1)		| A1_BASE: the GPU's RAM at $F03100, 32 bit writes
	move.l	#0x20000,0x24(%a1)	| A2_BASE
	move.l	#0x00002828,4(%a1)	| A1_FLAGS: PITCH1, PIXEL32, WID32, XADDPHR
	move.l	#0,0xc(%a1)		| A1_PIXEL
	move.l	#0x00002828,0x28(%a1)	| A2_FLAGS
	move.l	#0,0x30(%a1)		| A2_PIXEL
	move.l	#0x00010008,0x3c(%a1)	| B_COUNT: 1 line of 8
	move.l	#0x01800601,0x38(%a1)	| B_CMD: SRCEN, UPDA1, UPDA2, LFU_S
2:	move.l	0x38(%a1),%d1
	btst	#0,%d1
	beq.s	2b
	move.l	#0xf03100,0xf02110	| G_PC
	move.l	#1,0xf02114		| G_CTRL: GO
3:	tst.l	0x30020
	beq.s	3b
	stop	#0x2700

	.even
| movei #$30020,r0; movei #$c0ffee,r1; store r1,(r0); movei #G_CTRL,r3; moveq #0,r2;
| store r2,(r3); nop; nop (lyxass)
gpucode:
	.word	0x9800, 0x0020, 0x0003, 0x9801, 0xffee, 0x00c0, 0xbc01, 0x9803
	.word	0x2114, 0x00f0, 0x8c02, 0xbc62, 0xe400, 0xe400, 0x0000, 0x0000
