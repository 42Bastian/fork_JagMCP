| RUN: --m68k --cycles 14000 --mem $10000:$10 --mem $20000:$400 --mem $30000:$100
| The 68000 and the GPU on the bus together: the GPU stores and loads 200 long words in main RAM
| while the 68000 copies and adds words of its own there, the bus going to each by Tom's arbiter and
| the 68000's grant; the GPU then interrupts the 68000 (G_CTRL's CPUINT, INT1's GPU request),
| whose handler ends its loop.
	.text
	move.l	#handler,0x100		| vector 64
	move.w	#0x0002,0xf000e0	| INT1: the GPU's interrupt enabled
	lea	gpucode,%a0
	lea	0xf03000,%a1
	moveq	#15,%d0
1:	move.l	(%a0)+,(%a1)+
	dbra	%d0,1b
	move.l	#0xf03000,0xf02110	| G_PC
	move.l	#1,0xf02114		| G_CTRL: GO
	move.w	#0x2000,%sr
	moveq	#0,%d7
	moveq	#0,%d1
	lea	0x30000,%a2
2:	move.w	(%a2),%d2
	add.w	%d1,%d2
	move.w	%d2,2(%a2)
	addq.w	#1,%d1
	and.w	#0x7e,%d1
	lea	0x30000,%a2
	adda.w	%d1,%a2
	tst.w	%d7
	beq.s	2b
	move.w	#0x2700,%sr
	move.l	%d1,0x10004
	stop	#0x2700

handler:
	moveq	#1,%d7
	move.w	#0x0202,0xf000e0	| clear the GPU's request, keep it enabled
	move.w	#0,0xf000e2
	rte

	.even
| movei #$20000,r1; movei #$12345678,r2; movei #200,r3; .l: store r2,(r1); load (r1),r4;
| addq #4,r1; add r4,r2; subq #1,r3; jr ne,.l; nop; movei #$10000,r6; store r2,(r6);
| movei #G_CTRL,r4; moveq #3,r5; store r5,(r4); moveq #0,r5; store r5,(r4); nop; nop (lyxass)
gpucode:
	.word	0x9801, 0x0000, 0x0002, 0x9802, 0x5678, 0x1234, 0x9803, 0x00c8, 0x0000
	.word	0xbc22, 0xa424, 0x0881, 0x0082, 0x1823, 0xd741, 0xe400, 0x9806, 0x0000
	.word	0x0001, 0xbcc2, 0x9804, 0x2114, 0x00f0, 0x8c65, 0xbc85, 0x8c05, 0xbc85
	.word	0xe400, 0xe400, 0xe400, 0xe400, 0xe400
