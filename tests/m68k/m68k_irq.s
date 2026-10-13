| RUN: --m68k --cycles 14000 --mem $3f00:$1200
| Tom's interrupt on level 2: the PIT's, its handler counting in D7, clearing the request in INT1
| and resuming Tom's masters with INT2, returning with RTE; the vector is 64, which Tom puts on the
| bus in the acknowledge cycle. The program waits with STOP between interrupts, then with a loop,
| then masks them.
	.text
	move.l	#handler,0x100		| vector 64
	moveq	#0,%d7
	move.w	#0x0001,0xf00050	| PIT0: prescaler 1
	move.w	#0x0063,0xf00052	| PIT1: divider 99, an interrupt every 200 cycles
	move.w	#0x0008,0xf000e0	| INT1: the PIT's interrupt enabled
	stop	#0x2000			| wait for it, interrupts on
	stop	#0x2000
	move.w	#0x2000,%sr
1:	cmp.w	#6,%d7			| a few more in a loop
	blt.s	1b
	move.w	#0x2700,%sr
	move.l	%d7,dump
	move.w	0xf000e0,dump+4		| INT1's pending requests
	stop	#0x2700

handler:
	addq.w	#1,%d7
	move.w	#0x0808,0xf000e0	| clear the PIT's request, keep it enabled
	move.w	#0,0xf000e2		| INT2: resume
	rte

	.even
dump:	.space 16
