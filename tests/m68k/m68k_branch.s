| RUN: --m68k --cycles 8000 --mem $10000:$200
| Branches taken and not, short and long, DBcc loops to their end, BSR, JSR and RTS, JMP through
| a register, Scc.
	.text
	lea	0x10000,%a5
	lea	0x10100,%a4
	moveq	#5,%d0
	moveq	#0,%d1
1:	addq.l	#3,%d1
	dbra	%d0,1b
	move.l	%d1,(%a4)+
	moveq	#9,%d2
2:	addq.w	#1,%d3
	cmp.w	#4,%d3
	dbeq	%d2,2b
	move.l	%d2,(%a4)+
	tst.l	%d1
	beq.s	3f
	bne.w	4f
3:	moveq	#-1,%d4
4:	bsr.s	sub1
	jsr	sub2
	lea	5f,%a0
	jmp	(%a0)
	moveq	#-2,%d4
5:	cmp.l	%d1,%d2
	bgt.s	6f
	blt.w	6f
	moveq	#-3,%d4
6:	bcs.s	7f
	nop
7:	bra.s	8f
sub1:	addq.l	#1,%d5
	rts
sub2:	addq.l	#2,%d5
	bsr.w	sub1
	rts
8:	move.l	%d5,(%a4)+
	movem.l	%d0-%d7/%a0-%a6,(%a5)
	stop	#0x2700
