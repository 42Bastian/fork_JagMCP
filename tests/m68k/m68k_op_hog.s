| RUN: --m68k --cycles 20000 --mem $4000:$200 --mem $10000:$20
| An object list that never ends: three branch objects at $10000, on VC above, below and equal to
| YPOS, each back to the first. Once a line starts the object processor, it holds the bus for ever
| and the 68000 waits for its next bus cycle. The time base is video_border's, VDB at line 2.
	.text
	move.l	#0x00000020,0x10000	| branch: LINK $10000 (bits 42 to 24), YPOS $100, VC < YPOS
	move.l	#0x00004803,0x10004
	move.l	#0x00000020,0x10008	| VC > YPOS
	move.l	#0x00008803,0x1000c
	move.l	#0x00000020,0x10010	| VC = YPOS
	move.l	#0x00000803,0x10014
	move.l	#0x00010000,%d0		| OLP, its halves swapped
	swap	%d0
	move.l	%d0,0xf00020
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
	move.w	#2,0xf00046		| VDB
	move.w	#16,0xf00048		| VDE
	move.w	#0x6c1,0xf00028		| VMODE: the time base on
1:	addq.l	#1,%d7			| counts until the bus is gone
	move.l	%d7,0x10018
	bra.s	1b
