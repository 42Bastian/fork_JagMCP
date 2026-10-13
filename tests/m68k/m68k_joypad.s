| RUN: --m68k --cycles 5000 --mem $10000:$14
| The pads through Jerry's ports (jag_controller_mux.v): JOYSTICK's outputs enabled (bit 15) and
| each of the four columns selected at once in port 1 (the latch's low half) and port 2 (its high
| half, reversed), JOYSTICK and JOYBUTS read for each and stored in main RAM; then the outputs
| disabled and both read again. The simulation's pads are released; the control protocol's
| transcript (tests/control/lines.jsonl) holds buttons on them.
	.text
	.equ	JOY,0xf14000
	.equ	BUTS,0xf14002
	lea	0x10000,%a4
	lea	columns,%a0
	moveq	#3,%d0
1:	move.w	(%a0)+,JOY
	move.w	JOY,(%a4)+
	move.w	BUTS,(%a4)+
	dbra	%d0,1b
	move.w	#0x0100,JOY		| the outputs disabled
	move.w	JOY,(%a4)+
	move.w	BUTS,(%a4)+
	stop	#0x2700

	.even
| port 1's column 0 to 3 (latch bits 0 to 3 low), port 2's the same (latch bits 7 to 4 low)
columns:
	.word	0x817e, 0x81bd, 0x81db, 0x81e7
