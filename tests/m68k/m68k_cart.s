| RUN: --m68k --cycles 20000 --mem $10000:$200
| The 68000's reads of the cartridge, a 32 bit ROM blind to A0 and A1, and of the OS ROM, at each
| ROMWIDTH of MEMCON1 (8 to 32 bits), each ROMSPEED, FASTROM clear and set: bytes, words and long
| words at each offset; then code run from the cartridge, 32 bits wide.
	.text
	lea	0x10000,%a4
	lea	table,%a3
	moveq	#(tableEnd-table)/2-1,%d7
1:	move.w	(%a3)+,0xf00000
	lea	0x800010,%a0
	move.b	(%a0),(%a4)+
	move.b	1(%a0),(%a4)+
	move.b	2(%a0),(%a4)+
	move.b	7(%a0),(%a4)+
	move.w	(%a0),(%a4)+
	move.w	6(%a0),(%a4)+
	move.l	(%a0),(%a4)+
	move.l	6(%a0),(%a4)+
	move.l	0xe00008,(%a4)+
	move.b	0xe00009,(%a4)+
	move.b	0xe0000a,(%a4)+
	dbra	%d7,1b
	move.w	#0x1865,0xf00000
	jsr	cartcode
	move.l	%d0,(%a4)+
	move.l	%d1,(%a4)+
	move.w	#0x1861,0xf00000
	stop	#0x2700

table:	.word	0x1861,0x1863,0x1865		| 8, 16, 32 bits, the slowest
	.word	0x1869,0x1871,0x1879		| 8 bits, faster
	.word	0x187d,0x18e1,0x18fd		| 32 bits the fastest, FASTROM
tableEnd:

	.section .cart,"ax"
	.long	0x04040404
	.org	0x10
	.byte	0xa0,0xa1,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,0xa8,0xa9,0xaa,0xab,0xac,0xad,0xae,0xaf
	.org	0x100
cartcode:
	moveq	#5,%d0
	add.l	0x800014,%d0
	move.w	0x800012,%d1
	swap	%d1
	move.b	0x800019,%d1
	rts
