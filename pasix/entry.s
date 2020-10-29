/*.equ SPL,0x3d
.equ SPH,0x3e*/

.global _restore_stack,_dump_stack
_dump_stack:
	; copy current stack pointer and program counter to 0x0060

	in r16,0x3d
	in r17,0x3e
	sts 0x0060,r16
	sts 0x0061,r17

	ret

_restore_stack:
	; set stack pointer and program counter from 0x0060

	lds r16,0x0060
	lds r17,0x0061
	out 0x3d,r16
	out 0x3e,r17

	ret

.end