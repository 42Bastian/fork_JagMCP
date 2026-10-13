| RUN: --m68k --cycles 12000 --mem $3f00:$1200
| Exceptions: TRAP, TRAPV, CHK, a division by zero, ILLEGAL, the A and F lines, a privilege
| violation in user mode; each handler counts in D7, keeps what the 68000 stacked and returns with
| RTE. RTR, MOVE USP, and the user stack. (An address error is not modelled: the simulation's
| 68000 stacks the complement of the address.)
	.text
	move.w	#9,%d0			| vectors 2 to 11 and TRAP #3's to `handler`, in RAM
	lea	8,%a1
1:	move.l	#handler,(%a1)+
	dbra	%d0,1b
	move.l	#handler,0x8c
	move.l	#finish,0x90		| TRAP #4 ends the program in supervisor mode
	lea	0x3c00,%a2
	move.l	%a2,%usp
	lea	frames,%a6		| where the handler copies the stacked frames
	moveq	#0,%d7
	trap	#3
	move.w	#0x02,%ccr
	trapv
	moveq	#-5,%d1
	chk.w	#100,%d1
	moveq	#0,%d2
	divu.w	%d2,%d1
	.word	0x4afc			| ILLEGAL
	.word	0xa123			| line A
	.word	0xf456			| line F
	andi.w	#0xdfff,%sr		| user mode, on the user stack
	move.l	%usp,%a3		| privileged: a violation, stepped over
	pea	1f
	move.w	#0x0013,-(%sp)
	rtr
1:	trap	#3			| from user mode
	trap	#4
finish:	movem.l	%d0-%d7/%a0-%a6,dump
	stop	#0x2700

| an exception: count it, copy 8 words of the stack, step over the instruction if the stacked PC
| is the one that trapped (ILLEGAL, the lines, the privilege violation stack their own)
handler:
	addq.l	#1,%d7
	move.l	(%sp),(%a6)+
	move.l	4(%sp),(%a6)+
	move.l	8(%sp),(%a6)+
	move.l	12(%sp),(%a6)+
	cmp.l	#5,%d7
	blt.s	2f
	cmp.l	#8,%d7
	bgt.s	2f
	addq.l	#2,2(%sp)		| ILLEGAL, the lines, the privilege violation: past the instruction
2:	rte

	.even
frames:	.space 0x100
dump:	.space 64
