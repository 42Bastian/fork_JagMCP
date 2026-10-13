| RUN: --m68k --cycles 4000
| The 68000 stops the GPU and then the DSP while they run, by writes of G_CTRL and D_CTRL through
| their slave ports: each runs a loop of ADDQs, one issued nearly every cycle, so that the trace
| shows the last instruction issued and the last register written after the write is served.
	.text
	lea	riscode,%a0
	lea	0xf03000,%a1
	move.l	(%a0)+,(%a1)+
	move.l	(%a0)+,(%a1)+
	move.l	(%a0)+,(%a1)+
	move.l	(%a0)+,(%a1)+
	lea	riscode,%a0
	lea	0xf1b000,%a1
	moveq	#3,%d0
1:	move.l	(%a0)+,(%a1)+
	dbra	%d0,1b
	move.l	#0xf03000,0xf02110	| G_PC
	move.l	#1,0xf02114		| G_CTRL: GO
	move.l	#0xf1b000,0xf1a110	| D_PC
	move.l	#1,0xf1a114		| D_CTRL: GO
	nop
	nop
	move.l	#0,0xf02114		| G_CTRL: the GPU stops
	nop
	move.l	#0,0xf1a114		| D_CTRL: the DSP stops
	move.l	0xf02114,%d0
	move.l	0xf1a114,%d1
	stop	#0x2700

	.even
| .loop: addq #1,r1; addq #1,r2; addq #1,r3; addq #1,r4; addq #1,r5; jr .loop; addq #1,r6;
| nop (lyxass)
riscode:
	.word	0x0821, 0x0822, 0x0823, 0x0824, 0x0825, 0xd740, 0x0826, 0xe400
