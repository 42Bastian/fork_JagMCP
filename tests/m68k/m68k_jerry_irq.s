| RUN: --m68k --cycles 20000 --mem $3f00:$1200
| Jerry's interrupts to the 68000, through INT1's bit 4: Jerry's timer 1, enabled in JINTCTRL,
| then the DSP's interrupt to the CPU (D_CTRL's CPUINT written by the 68000 through the DSP's
| slave port). The handler stores JINTCTRL's latches, acknowledges them there and in INT1, resumes
| Tom's masters with INT2 and counts in D7.
	.text
	move.l	#handler,0x100		| vector 64
	lea	log,%a4
	moveq	#0,%d7
	move.w	#0x0010,0xf000e0	| INT1: Jerry's interrupt enabled
	moveq	#4,%d6			| JINTCTRL: timer 1
	move.w	%d6,0xf10020
	move.w	#49,0xf10000		| JPIT1: prescaler 49
	move.w	#9,0xf10002		| JPIT2: divider 9, a request every 500 cycles
	stop	#0x2000
	stop	#0x2000
	move.w	#0x2000,%sr
1:	cmp.w	#3,%d7
	blt.s	1b
	move.w	#0x2700,%sr
	move.w	#0,0xf10000		| the timer stopped
	moveq	#2,%d6			| JINTCTRL: the DSP's interrupt
	move.w	%d6,0xf10020
	move.w	#0x0402,0xf10020	| and timer 1's latch acknowledged, should it be set
	move.w	%d6,0xf10020
	move.w	#0x0010,0xf000e0
	move.w	#0x2000,%sr
	move.l	#2,0xf1a114		| D_CTRL: CPUINT
2:	cmp.w	#4,%d7
	blt.s	2b
	move.l	#2,0xf1a114
	stop	#0x2000
	move.w	#0x2700,%sr
	move.l	%d7,dump
	move.w	0xf000e0,dump+4		| INT1's pending requests
	move.w	0xf10020,dump+6		| JINTCTRL's latches
	stop	#0x2700

handler:
	move.w	0xf10020,%d0		| the latches
	move.w	%d0,(%a4)+
	lsl.w	#8,%d0			| acknowledged, the enables kept
	or.w	%d6,%d0
	move.w	%d0,0xf10020
	move.w	#0x1010,0xf000e0	| INT1: Jerry's request cleared, kept enabled
	move.w	#0,0xf000e2		| INT2: resume
	addq.w	#1,%d7
	rte

	.even
dump:	.space 16
log:	.space 32
