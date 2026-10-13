| RUN: --m68k --cycles 9000 --mem $30000:$40
| The 68000 reaches into the DSP's local space while the DSP clears its RAM in a loop (a store a
| few cycles): bytes, words and long words of RAM and D_CTRL read, at phases the loop's delays
| vary. Each read takes the DSP's local bus for a cycle (ioreq), which the DSP's counter shows.
| The DSP stops itself once the 68000 has set the flag it polls.
	.text
	lea	dspcode,%a0
	lea	0xf1b000,%a1
	moveq	#17,%d0
1:	move.l	(%a0)+,(%a1)+
	dbra	%d0,1b
	move.l	#0,0xf1c200		| the flag the DSP polls
	move.l	#0xf1b000,0xf1a110	| D_PC
	move.l	#1,0xf1a114		| D_CTRL: GO
	lea	0x30004,%a2
	moveq	#11,%d7
2:	move.b	0xf1c0f0,%d0		| a byte
	move.w	%d7,%d1			| a delay that moves the next reads' phase
3:	dbra	%d1,3b
	move.w	0xf1c0f4,%d0		| a word
	move.l	0xf1c0f8,%d0		| a long word
	move.l	0xf1a114,%d0		| D_CTRL
	dbra	%d7,2b
	move.l	#1,0xf1c200		| the DSP stops
4:	tst.l	0x30000
	beq.s	4b
	stop	#0x2700

	.even
| moveq #0,r1; moveq #0,r3; movei #.outer,r6; .outer: movei #$f1c100,r14; movei #$100,r0;
| .clear: store r1,(r14); subq #4,r14; subq #4,r0; jr ne,.clear; nop; addq #1,r3;
| movei #$f1c200,r2; load (r2),r4; cmpq #0,r4; jump eq,(r6); nop; movei #$30000,r4;
| store r3,(r4); movei #D_CTRL,r4; moveq #0,r5; store r5,(r4); nop; nop (lyxass)
dspcode:
	.word	0x8c01, 0x8c03, 0x9806, 0xb00a, 0x00f1, 0x980e, 0xc100, 0x00f1, 0x9800, 0x0100
	.word	0x0000, 0xbdc1, 0x188e, 0x1880, 0xd781, 0xe400, 0x0823, 0x9802, 0xc200, 0x00f1
	.word	0xa444, 0x7c04, 0xd0c2, 0xe400, 0x9804, 0x0000, 0x0003, 0xbc83, 0x9804, 0xa114
	.word	0x00f1, 0x8c05, 0xbc85, 0xe400, 0xe400, 0x0000
