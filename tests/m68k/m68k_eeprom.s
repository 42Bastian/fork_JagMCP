| RUN: --m68k --cycles 60000 --mem $10000:$40
| The cartridge's EEPROM, a 93C46 on Jerry's chip selects: each command after CS has been dropped
| by an access to GPIO1, its bits written to GPIO0 (the clock as each write ends, the data its
| bit 0), DO read in JOYSTICK's bit 0 after each clock of GPIO0 read. EWEN, WRITE, READ back,
| READ of a word erased, EWDS and a WRITE ignored, ERASE, WRAL; each word read stored in main RAM.
	.text
	.equ	GPIO0,0xf14800
	.equ	GPIO1,0xf15000
	.equ	JOY,0xf14000
	lea	0x10000,%a4
	move.w	#0b100110000,%d0	| EWEN
	bsr	command
	move.w	#0b101000101,%d0	| WRITE 5
	bsr	command
	move.w	#0xa5c3,%d0
	moveq	#16,%d1
	bsr	send
	tst.w	GPIO1
	move.w	JOY,(%a4)+		| DO: ready
	moveq	#5,%d0
	bsr	read
	moveq	#6,%d0
	bsr	read
	move.w	#0b100000000,%d0	| EWDS
	bsr	command
	move.w	#0b101000110,%d0	| WRITE 6, not done
	bsr	command
	move.w	#0x1234,%d0
	moveq	#16,%d1
	bsr	send
	moveq	#6,%d0
	bsr	read
	move.w	#0b100110000,%d0	| EWEN
	bsr	command
	move.w	#0b111000101,%d0	| ERASE 5
	bsr	command
	moveq	#5,%d0
	bsr	read
	move.w	#0b100010000,%d0	| WRAL
	bsr	command
	move.w	#0x5a3c,%d0
	moveq	#16,%d1
	bsr	send
	moveq	#63,%d0
	bsr	read
	moveq	#5,%d0
	bsr	read
	tst.w	GPIO1
	stop	#0x2700

| a command of nine bits, CS dropped before it
command:
	tst.w	GPIO1
	moveq	#9,%d1
| the low d1 bits of d0, the highest first
send:
	subq.w	#1,%d1
1:	move.w	%d0,%d2
	lsr.w	%d1,%d2
	move.w	%d2,GPIO0
	dbra	%d1,1b
	rts

| READ of the word d0, stored
read:
	or.w	#0b110000000,%d0
	bsr	command
	moveq	#15,%d1
	moveq	#0,%d3
1:	tst.w	GPIO0
	move.w	JOY,%d2
	lsr.w	#1,%d2
	addx.w	%d3,%d3
	dbra	%d1,1b
	move.w	%d3,(%a4)+
	rts
