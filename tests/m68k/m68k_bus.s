| RUN: --m68k --cycles 6000 --mem $10000:$200
| The 68000's cycles to every memory: the ROM (8 bits wide, the slowest speed), MEMCON1 and
| MEMCON2 read back, Jerry's joystick ports, DRAM bytes at even and odd addresses, TAS.
	.text
	lea	0x10000,%a5
	lea	0x10100,%a4
	move.l	0xe00000,(%a4)+
	move.b	0xe00009,(%a4)+
	move.b	0xe0000a,(%a4)+
	move.w	0xf00000,(%a4)+
	move.w	0xf00002,(%a4)+
	move.w	0xf14000,(%a4)+
	move.w	0xf14002,(%a4)+
	move.b	#0x11,0x10181
	move.b	#0x22,0x10182
	move.w	#0x3344,0x10184
	move.l	0x10180,(%a4)+
	clr.b	0x10190
	tas	0x10190
	smi	(%a4)+
	tas	0x10190
	smi	(%a4)+
	movem.l	%d0-%d7/%a0-%a6,(%a5)
	stop	#0x2700
