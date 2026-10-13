| RUN: --m68k --cycles 7000
| The 68000 writes G_FLAGS while the GPU polls its local RAM (MOVEI, a load, CMPQ, a JR and a
| MOVEQ in its delay slot) and the DSP runs a loop of its own beside it: Z set and cleared in
| turn, a NOP more between the writes each time, so that they land at every phase of the loop.
| Each is served by the GPU's memory controller a cycle after the port asks for its local bus;
| the trace shows the cycle the flags change in. The 68000 stops both at the end.
	.text
	lea	dspcode,%a0
	lea	0xf1b000,%a1
	moveq	#3,%d0
1:	move.l	(%a0)+,(%a1)+
	dbra	%d0,1b
	move.l	#0,0xf1b100
	lea	gpucode,%a0
	lea	0xf03000,%a1
	moveq	#3,%d0
2:	move.l	(%a0)+,(%a1)+
	dbra	%d0,2b
	move.l	#0,0xf03020		| the word the GPU polls
	move.l	#0xf1b000,0xf1a110	| D_PC
	move.l	#1,0xf1a114		| D_CTRL: GO
	move.l	#0xf03000,0xf02110	| G_PC
	move.l	#1,0xf02114		| G_CTRL: GO
	moveq	#0,%d1
	moveq	#9,%d0
3:	move.l	#1,0xf02100		| G_FLAGS: Z
	move.w	%d1,%d2
4:	dbra	%d2,4b
	move.l	#0,0xf02100
	move.w	%d1,%d2
5:	nop
	dbra	%d2,5b
	addq.w	#1,%d1
	dbra	%d0,3b
	move.w	#1,0xf02102		| the low half alone
	nop
	move.w	#0,0xf02102
	move.l	#0,0xf02114		| G_CTRL: the GPU stops
	move.l	#0,0xf1a114		| D_CTRL: the DSP stops
	stop	#0x2700

	.even
| .loop: movei #$f03020,r2; load (r2),r3; cmpq #0,r3; jr eq,.loop; moveq #0,r1; nop (lyxass)
gpucode:
	.word	0x9802, 0x3020, 0x00f0, 0xa443, 0x7c03, 0xd742, 0x8c01, 0xe400
| movei #$f1b100,r14; .loop: load (r14),r3; addq #1,r4; store r4,(r14+4); jr .loop; nop (lyxass)
dspcode:
	.word	0x980e, 0xb100, 0x00f1, 0xa5c3, 0x0824, 0xc424, 0xd780, 0xe400
