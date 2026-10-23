; SPDX-FileCopyrightText: 2026 Chris Watson
; SPDX-License-Identifier: GPL-3.0-or-later
	.module crt0
	.globl	_main
	.globl	_vblank_handler
	.globl	___sdcc_bcall_ehl
	.globl	_rom_bank

	INT_CTRL = 0xF814
	BANK_REG = 0xF818
	HOME_BANK = 2
	STACK_TOP = 0xD000

	.area	_HEADER (ABS)
	.org	0x0000
	di
	ld	sp, #STACK_TOP
	im	1
	jp	init

	.org	0x0038
	push	af
	push	bc
	push	de
	push	hl
	push	ix
	push	iy
	call	_vblank_handler
	xor	a, a
	ld	(INT_CTRL), a
	inc	a
	ld	(INT_CTRL), a
	pop	iy
	pop	ix
	pop	hl
	pop	de
	pop	bc
	pop	af
	ei
	reti

	.org	0x0066
	retn

___sdcc_bcall_ehl:
	ld	a, (_rom_bank)
	push	af
	inc	sp
	ld	a, e
	ld	(_rom_bank), a
	ld	(BANK_REG), a
	call	bcall_hl
	ld	c, a
	dec	sp
	pop	af
	ld	(_rom_bank), a
	ld	(BANK_REG), a
	ld	a, c
	ret
bcall_hl:
	jp	(hl)

	.area	_HOME
	.area	_CODE
	.area	_INITIALIZER
	.area	_GSINIT
	.area	_GSFINAL
	.area	_DATA
	.area	_INITIALIZED
	.area	_BSEG
	.area	_BSS
	.area	_HEAP

	.area	_DATA
_rom_bank:
	.ds	1

	.area	_CODE
init:
	call	gsinit
	ld	a, #HOME_BANK
	ld	(_rom_bank), a
	ld	(BANK_REG), a
	call	_main
halt_loop:
	halt
	jr	halt_loop

	.area	_GSINIT
gsinit:
	ld	bc, #l__DATA
	ld	a, b
	or	a, c
	jr	z, data_zeroed
	ld	hl, #s__DATA
	ld	(hl), #0
	dec	bc
	ld	a, b
	or	a, c
	jr	z, data_zeroed
	ld	e, l
	ld	d, h
	inc	de
	ldir
data_zeroed:
	ld	bc, #l__INITIALIZER
	ld	a, b
	or	a, c
	jr	z, gsinit_next
	ld	de, #s__INITIALIZED
	ld	hl, #s__INITIALIZER
	ldir
gsinit_next:

	.area	_GSFINAL
	ret
