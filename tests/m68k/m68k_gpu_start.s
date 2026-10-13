| RUN: --m68k --cycles 5000 --mem $10000:$10
| The 68000 sets the GPU going: it copies the GPU's program into the GPU's RAM a long word at a time
| (two word cycles each, through the GPU's slave port), writes G_PC and G_CTRL's GO, and polls main
| RAM until the GPU has counted to 10 there; the GPU then clears its own GO.
	.text
	lea	gpucode,%a0
	lea	0xf03000,%a1
	moveq	#8,%d0
1:	move.l	(%a0)+,(%a1)+
	dbra	%d0,1b
	move.l	#0xf03000,0xf02110	| G_PC
	move.l	#1,0xf02114		| G_CTRL: GO
2:	cmp.l	#10,0x10000
	bne.s	2b
	move.l	0xf02114,0x10004	| G_CTRL read back
	stop	#0x2700

	.even
| movei #$10000,r1; moveq #0,r2; moveq #10,r3; .l: addq #1,r2; store r2,(r1); subq #1,r3;
| jr ne,.l; nop; movei #G_CTRL,r4; moveq #0,r5; store r5,(r4); nop; nop (lyxass)
gpucode:
	.word	0x9801, 0x0000, 0x0001, 0x8c02, 0x8d43, 0x0822, 0xbc22, 0x1823, 0xd781
	.word	0xe400, 0x9804, 0x2114, 0x00f0, 0x8c05, 0xbc85, 0xe400, 0xe400, 0xe400
