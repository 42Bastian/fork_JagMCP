| RUN: --m68k --cycles 6000 --mem $10000:$200
| Shifts and rotates by immediates and by register counts (their time is the count's), and of
| memory words; the bit operations.
	.text
	lea	0x10000,%a5
	lea	0x10100,%a4
	move.l	#0x81234567,%d0
	moveq	#13,%d1
	moveq	#63,%d2
	lsl.l	%d1,%d0
	move.l	%d0,(%a4)+
	asr.w	%d2,%d0
	move.w	%sr,(%a4)+
	ror.b	#3,%d0
	roxl.l	#8,%d0
	rol.w	%d1,%d0
	lsr.l	#1,%d0
	asl.l	%d2,%d0
	move.w	%sr,(%a4)+
	roxr.w	%d1,%d0
	move.l	%d0,(%a4)+
	move.w	#0xc001,0x10180
	asl.w	0x10180
	lsr.w	0x10180
	ror.w	0x10180
	roxl.w	0x10180
	move.l	#0x0f0f0f0f,%d3
	btst	#4,%d3
	sne	(%a4)+
	seq	(%a4)+
	bset	%d1,%d3
	bclr	#0,%d3
	bchg	%d2,%d3
	move.l	%d3,(%a4)+
	bset	#3,0x10181
	btst	%d1,0x10181
	seq	(%a4)+
	movem.l	%d0-%d7/%a0-%a6,(%a5)
	stop	#0x2700
