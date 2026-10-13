| RUN: --m68k --cycles 5000 --mem $10000:$10
| The 68000 sets the DSP going: it copies the DSP's program into the DSP's RAM through Jerry's slave
| (two word cycles a long word), writes D_PC and D_CTRL's GO, and polls main RAM until the DSP has
| counted to 10 there; the DSP then clears its own GO, which the 68000 reads back.
	.text
	lea	dspcode,%a0
	lea	0xf1b000,%a1
	moveq	#9,%d0
1:	move.l	(%a0)+,(%a1)+
	dbra	%d0,1b
	move.l	#0xf1b000,0xf1a110	| D_PC
	move.l	#1,0xf1a114		| D_CTRL: GO
2:	cmp.l	#10,0x10000
	bne.s	2b
3:	move.l	0xf1a114,%d0		| D_CTRL, read back through Jerry's slave until GO is clear
	btst	#0,%d0
	bne.s	3b
	move.l	%d0,0x10004
	move.l	0xf1b004,0x10008	| a long word of the DSP's RAM
	stop	#0x2700

	.even
| movei #$10000,r1; moveq #0,r2; moveq #10,r3; .l: addq #1,r2; store r2,(r1); subq #1,r3;
| jr ne,.l; nop; movei #D_CTRL,r4; moveq #0,r5; store r5,(r4); nop; nop (lyxass)
dspcode:
	.word	0x9801, 0x0000, 0x0001, 0x8c02, 0x8d43, 0x0822, 0xbc22, 0x1823, 0xd781
	.word	0xe400, 0x9804, 0xa114, 0x00f1, 0x8c05, 0xbc85, 0xe400, 0xe400, 0xe400
	.word	0xe400, 0xe400
