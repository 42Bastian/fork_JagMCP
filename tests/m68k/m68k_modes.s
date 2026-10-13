| RUN: --m68k --cycles 8000 --mem $10000:$400
| Every addressing mode, of bytes, words and long words, as source and destination: predecrement
| and postincrement (A7 by two for a byte), displacements, indices of both sizes, absolute short
| and long, PC relative; LEA, PEA, MOVEM both ways, LINK and UNLK, MOVEP.
	.text
	lea	0x10000,%a5
	lea	0x10200,%a0
	lea	0x10280,%a1
	move.l	#0x01020304,(%a0)+
	move.w	#0x0506,(%a0)+
	move.b	#7,(%a0)+
	move.b	#8,(%a0)+
	move.l	-(%a0),%d0
	move.w	-4(%a0),%d1
	moveq	#2,%d2
	move.b	3(%a0,%d2.w),%d3
	move.l	#-2,%d4
	move.w	6(%a0,%d4.l),%d5
	move.l	%d0,8(%a1)
	move.w	%d1,-2(%a1,%d2.w)
	move.b	%d3,(%a1)
	move.l	(%a1)+,(%a0)+
	move.w	(0x10200).w,%d6
	move.l	(0x10204).l,%d7
	move.w	data(%pc),(%a1)+
	move.l	data(%pc,%d2.w),(%a1)+
	lea	4(%a0,%d2.l),%a2
	pea	(%a2)
	move.l	(%sp)+,%a3
	move.b	#0x55,-(%sp)
	move.b	(%sp)+,%d6
	movem.l	%d0-%d3/%a0-%a1,-(%sp)
	movem.w	(%sp)+,%d4-%d7
	movem.l	(%sp)+,%d0-%d1
	link	%a6,#-8
	move.l	%d0,-4(%a6)
	unlk	%a6
	movep.l	%d0,1(%a1)
	movep.w	1(%a1),%d7
	move.l	%sp,%a4
	movem.l	%d0-%d7/%a0-%a6,(%a5)
	stop	#0x2700
	.even
data:	.long	0xdeadbeef,0xcafebabe,0x0badf00d
