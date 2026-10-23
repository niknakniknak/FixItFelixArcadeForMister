; SPDX-FileCopyrightText: 2026 Chris Watson
; SPDX-License-Identifier: GPL-3.0-or-later
	.module bigspr
	.globl	_big_put
	.globl	_big_clear
	.globl	_big_part
	.globl	_big_n
	.globl	_big_x
	.globl	_big_y
	.globl	_big_first
	.globl	_big_cnt
	.globl	_big_used
	.globl	_big_list
	.globl	_spr_big_flags
	.globl	_spr_out
	.globl	_spr_free

	SIZE_PAGE = 0xF2
	LIST_MAX = 96
	LIMIT_X = 239
	LIMIT_Y = 0x13F

	.area	_DATA
_big_part::	.ds	2
_big_n::	.ds	1
_big_x::	.ds	2
_big_y::	.ds	2
_big_first::	.ds	1
_big_cnt::	.ds	1
_big_used::	.ds	1
_big_list::	.ds	LIST_MAX
_spr_big_flags::	.ds	1
bp_sx:		.ds	2
bp_sy:		.ds	2
bp_idx:		.ds	1

	.area	_CODE
bp_sizes:
	.db	16, 32, 64, 64

_big_put::
	ld	a, (_big_n)
	or	a, a
	ret	z
	push	ix
	ld	ix, (_big_part)
	ld	de, (_spr_out)
bp_loop:
	ld	a, (_spr_free)
	or	a, a
	jp	z, bp_done
	ld	a, 4 (ix)
	and	a, #3
	ld	c, a
	ld	b, #0
	ld	hl, #bp_sizes
	add	hl, bc
	ld	c, (hl)
	ld	hl, (_big_x)
	ld	a, 0 (ix)
	add	a, l
	ld	l, a
	adc	a, h
	sub	a, l
	ld	h, a
	ld	(bp_sx), hl
	add	hl, bc
	dec	hl
	ld	a, c
	add	a, #LIMIT_X
	ld	c, a
	ld	a, b
	adc	a, #0
	ld	b, a
	or	a, a
	sbc	hl, bc
	jp	nc, bp_skip
	ld	a, 4 (ix)
	rrca
	rrca
	and	a, #3
	ld	c, a
	ld	b, #0
	ld	hl, #bp_sizes
	add	hl, bc
	ld	c, (hl)
	ld	hl, (_big_y)
	ld	a, 1 (ix)
	add	a, l
	ld	l, a
	adc	a, h
	sub	a, l
	ld	h, a
	ld	(bp_sy), hl
	add	hl, bc
	dec	hl
	ld	a, c
	add	a, #<LIMIT_Y
	ld	c, a
	ld	a, #>LIMIT_Y
	adc	a, #0
	ld	b, a
	or	a, a
	sbc	hl, bc
	jp	nc, bp_skip
	ld	a, (bp_sx)
	ld	(de), a
	inc	de
	ld	a, (bp_sy)
	ld	(de), a
	inc	de
	ld	a, 2 (ix)
	ld	(de), a
	inc	de
	ld	a, (bp_sy + 1)
	and	a, #1
	add	a, a
	add	a, a
	or	a, 3 (ix)
	ld	(de), a
	ld	a, e
	rrca
	rrca
	and	a, #0x3F
	ld	l, a
	ld	a, d
	and	a, #1
	rrca
	rrca
	or	a, l
	ld	l, a
	ld	(bp_idx), a
	ld	h, #SIZE_PAGE
	ld	a, (bp_sx + 1)
	and	a, #1
	rlca
	rlca
	rlca
	rlca
	ld	b, a
	ld	a, (_spr_big_flags)
	or	a, b
	or	a, 4 (ix)
	ld	(hl), a
	ld	a, (_big_used)
	cp	a, #LIST_MAX
	jr	nc, bp_norec
	ld	c, a
	inc	a
	ld	(_big_used), a
	ld	b, #0
	ld	hl, #_big_list
	add	hl, bc
	ld	a, (bp_idx)
	ld	(hl), a
bp_norec:
	inc	de
	ld	hl, #_spr_free
	dec	(hl)
bp_skip:
	ld	bc, #5
	add	ix, bc
	ld	hl, #_big_n
	dec	(hl)
	jp	nz, bp_loop
bp_done:
	ld	(_spr_out), de
	pop	ix
	ret

_big_clear::
	ld	a, (_big_used)
	or	a, a
	ret	z
	ld	b, a
	ld	c, #0
	ld	hl, #_big_list
	ld	de, #_big_list
bc_loop:
	ld	a, (hl)
	inc	hl
	push	hl
	ld	l, a
	ld	a, (_big_first)
	ld	h, a
	ld	a, l
	sub	a, h
	ld	h, a
	ld	a, (_big_cnt)
	cp	a, h
	jr	c, bc_keep
	jr	z, bc_keep
	ld	h, #SIZE_PAGE
	xor	a, a
	ld	(hl), a
	pop	hl
	djnz	bc_loop
	jr	bc_end
bc_keep:
	ld	a, l
	ld	(de), a
	inc	de
	inc	c
	pop	hl
	djnz	bc_loop
bc_end:
	ld	a, c
	ld	(_big_used), a
	ret
