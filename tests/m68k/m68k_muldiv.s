| RUN: --m68k --cycles 8000 --mem $10000:$200
| MULU, MULS, DIVU and DIVS: their times depend on the operands; a division overflows and by zero
| would trap (not here: the vectors are the ROM's).
	.text
	lea	0x10000,%a5
	lea	0x10100,%a4
	move.l	#0x0000ffff,%d0
	mulu.w	%d0,%d0
	move.l	%d0,(%a4)+
	move.w	#0x5555,%d1
	mulu.w	#0xaaaa,%d1
	move.l	%d1,(%a4)+
	move.w	#0x8000,%d2
	muls.w	#-1,%d2
	move.l	%d2,(%a4)+
	move.w	#0x1234,%d3
	muls.w	%d3,%d3
	move.l	%d3,(%a4)+
	move.l	#1000000,%d4
	divu.w	#7,%d4
	move.l	%d4,(%a4)+
	move.l	#-1000000,%d5
	divs.w	#-13,%d5
	move.l	%d5,(%a4)+
	move.l	#0x12345678,%d6
	divu.w	#2,%d6
	move.w	%sr,(%a4)+
	move.l	%d6,(%a4)+
	move.l	#0x7fffffff,%d7
	divs.w	#0x7fff,%d7
	move.l	%d7,(%a4)+
	moveq	#0,%d0
	mulu.w	#0,%d0
	move.l	%d0,(%a4)+
	move.l	#0xffff0000,%d1
	divu.w	#0xffff,%d1
	move.l	%d1,(%a4)+
	movem.l	%d0-%d7/%a0-%a6,(%a5)
	stop	#0x2700
