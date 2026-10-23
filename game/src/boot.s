; SPDX-FileCopyrightText: 2026 Chris Watson
; SPDX-License-Identifier: GPL-3.0-or-later
	.module boot
	.globl	_garbage_fill
	.globl	_rom_sum_slice
	.globl	_garbage_table
	.globl	_rom_bank

	BANK_REG = 0xF818
	GARBAGE_BANK = 5
	FG_BASE = 0xE000
	GARBAGE_BLOCKS = 160
	WINDOW = 0x8000

	.area	_CODE
_garbage_fill::
	ld	a, (_rom_bank)
	push	af
	ld	a, #GARBAGE_BANK
	ld	(_rom_bank), a
	ld	(BANK_REG), a
	add	hl, hl
	ld	de, #_garbage_table
	add	hl, de
	ld	de, #FG_BASE
	ld	a, #GARBAGE_BLOCKS
gf_loop:
	ldi
	ldi
	ldi
	ldi
	ldi
	ldi
	ldi
	ldi
	ldi
	ldi
	ldi
	ldi
	ldi
	ldi
	ldi
	ldi
	dec	a
	jp	nz, gf_loop
	pop	af
	ld	(_rom_bank), a
	ld	(BANK_REG), a
	ret

_rom_sum_slice::
	ld	c, a
	ld	a, (_rom_bank)
	push	af
	ld	a, c
	and	a, #0x18
	jr	z, rs_fixed
	ld	a, c
	rrca
	rrca
	and	a, #0x07
	ld	(_rom_bank), a
	ld	(BANK_REG), a
	ld	a, c
	and	a, #0x03
	add	a, a
	add	a, a
	add	a, a
	add	a, a
	add	a, #>WINDOW
	jr	rs_go
rs_fixed:
	ld	a, c
	add	a, a
	add	a, a
	add	a, a
	add	a, a
rs_go:
	ld	h, a
	ld	l, #0
	xor	a, a
	ld	b, #0
rs_loop:
	add	a, (hl)
	inc	hl
	add	a, (hl)
	inc	hl
	add	a, (hl)
	inc	hl
	add	a, (hl)
	inc	hl
	add	a, (hl)
	inc	hl
	add	a, (hl)
	inc	hl
	add	a, (hl)
	inc	hl
	add	a, (hl)
	inc	hl
	add	a, (hl)
	inc	hl
	add	a, (hl)
	inc	hl
	add	a, (hl)
	inc	hl
	add	a, (hl)
	inc	hl
	add	a, (hl)
	inc	hl
	add	a, (hl)
	inc	hl
	add	a, (hl)
	inc	hl
	add	a, (hl)
	inc	hl
	djnz	rs_loop
	ld	c, a
	pop	af
	ld	(_rom_bank), a
	ld	(BANK_REG), a
	ld	a, c
	ret
