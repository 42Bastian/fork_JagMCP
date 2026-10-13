| RUN: --m68k --cycles 6000 --mem $30000:$20
| The 68000 reaches into the GPU's local space while the GPU runs a loop on its local RAM (an
| MMULT reading its matrix a word a cycle, a load, a store, its fetches): reads of G_FLAGS, G_CTRL
| and RAM, writes of RAM. Each takes the GPU's local bus for a cycle (ioreq), which holds MMULT's
| sequence back and shows in the GPU's counter. The GPU stops itself once the 68000 has set the
| flag it polls.
	.text
	lea	gpucode,%a0
	lea	0xf03000,%a1
	moveq	#16,%d0
1:	move.l	(%a0)+,(%a1)+
	dbra	%d0,1b
	move.l	#0,0xf03800		| the flag the GPU polls (high half first, as the port needs)
	move.l	#0xf03000,0xf02110	| G_PC
	move.l	#1,0xf02114		| G_CTRL: GO
	move.l	0xf02100,%d1		| G_FLAGS
	move.l	0xf02114,%d2		| G_CTRL
	move.l	0xf03804,%d3		| the GPU's counter
	move.w	0xf03806,%d4
	move.l	%d3,0xf03808		| RAM written
	move.w	%d4,0xf0380e
	move.l	0xf03804,%d5
	move.l	#1,0xf03800		| the GPU stops
2:	tst.l	0x30000
	beq.s	2b
	movem.l	%d3-%d5,0x30004
	move.l	0xf03808,0x30010
	move.l	0xf0380c,0x30014
	stop	#0x2700

	.even
| movei #G_MTXC,r4; moveq #8,r5; store r5,(r4); movei #G_MTXA,r4; movei #$f03900,r5;
| store r5,(r4); movei #$f03800,r14; moveq #0,r3; .loop: mmult r0,r6; load (r14),r2; addq #1,r3;
| store r3,(r14+4); cmpq #0,r2; jr eq,.loop; nop; movei #$30000,r4; store r3,(r4);
| movei #G_CTRL,r4; moveq #0,r5; store r5,(r4); nop; nop (lyxass)
gpucode:
	.word	0x9804, 0x2104, 0x00f0, 0x8d05, 0xbc85, 0x9804, 0x2108, 0x00f0, 0x9805, 0x3900
	.word	0x00f0, 0xbc85, 0x980e, 0x3800, 0x00f0, 0x8c03, 0xd806, 0xa5c2, 0x0823, 0xc423
	.word	0x7c02, 0xd742, 0xe400, 0x9804, 0x0000, 0x0003, 0xbc83, 0x9804, 0x2114, 0x00f0
	.word	0x8c05, 0xbc85, 0xe400, 0xe400
