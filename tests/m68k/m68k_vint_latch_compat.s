| RUN: --m68k --cycles 20000 --mem $4000:$200 --compat
| The vertical interrupt latched while it is disabled (misc.v lvint), then enabled by a write of
| INT1 that does not clear it, as Native and Barkley Shut Up and Jam do: with the MiSTer core's
| vintbugfix (--compat) the write clears the latch, the next VI interrupts the loop that waits for
| it; m68k_vint_latch is the same program on the netlist, where no rise of the latch follows and
| the 68000 waits for ever. The time base is video_border's, short lines and fields, VI on line 11.
	.text
	move.l	#handler,0x100		| vector 64
	moveq	#0,%d7
	move.w	#99,0xf0002e		| HP
	move.w	#0x400+80,0xf00030	| HBB
	move.w	#10,0xf00032		| HBE
	move.w	#0x400+90,0xf00034	| HS
	move.w	#20,0xf00038		| HDB1
	move.w	#20,0xf0003a		| HDB2
	move.w	#0x400+70,0xf0003c	| HDE
	move.w	#19,0xf0003e		| VP
	move.w	#17,0xf00040		| VBB
	move.w	#2,0xf00042		| VBE
	move.w	#18,0xf00044		| VS
	move.w	#0x7ff,0xf00046		| VDB
	move.w	#0x7ff,0xf00048		| VDE
	move.w	#11,0xf0004e		| VI
	move.w	#0x6c1,0xf00028		| VMODE: the time base on
	move.w	#300,%d0		| a few fields with VI disabled: its latch set
1:	dbra	%d0,1b
	move.w	#0x0001,0xf000e0	| INT1: VI enabled, its latch not cleared
	move.w	#0x2000,%sr		| interrupts on, three of them waited for
2:	cmp.w	#3,%d7
	blt.s	2b
	move.w	#0x2700,%sr
	move.l	%d7,dump
	stop	#0x2700

handler:
	addq.w	#1,%d7
	move.w	#0x0101,0xf000e0	| clear VI's request, keep it enabled
	move.w	#0,0xf000e2		| INT2: resume
	rte

	.even
dump:	.space 16
