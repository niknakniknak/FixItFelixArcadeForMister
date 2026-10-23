; SPDX-FileCopyrightText: 2026 Chris Watson
; SPDX-License-Identifier: GPL-3.0-or-later
	.module bgfast
	.globl	_tmr_decode
	.globl	_tmr_put
	.globl	_tmr_clear
	.globl	_tmr_src
	.globl	_tmr_w
	.globl	_tmr_buf
	.globl	_tmr_dst
	.globl	_tmr_i0
	.globl	_tmr_n
	.globl	_tmr_c0
	.globl	_tmr_opaque
	.globl	_tmr_draw
	.globl	_tmr_tm
	.globl	_tmr_row
	.globl	_tmr_col
	.globl	_tmr_lo
	.globl	_tmr_hi
	.globl	_tmr_last
	.globl	_tilemaps
	.globl	_tm_rows
	.globl	_fcr_fill
	.globl	_fcr_first
	.globl	_fcr_n
	.globl	_sts_ptr
	.globl	_sts_n
	.globl	_sts_dx
	.globl	_sts_size
	.globl	_st_shift

	ROW_CELLS = 32
	FG_BASE = 0xE000

	.area	_DATA
_tmr_src::	.ds	2
_tmr_w::	.ds	1
_tmr_buf::	.ds	64
_tmr_dst::	.ds	2
_tmr_i0::	.ds	1
_tmr_n::	.ds	1
_tmr_c0::	.ds	1
_tmr_opaque::	.ds	1
_tmr_tm::	.ds	1
_tmr_row::	.ds	1
_tmr_col::	.ds	2
_tmr_lo::	.ds	2
_tmr_hi::	.ds	2
_tmr_last::	.ds	2
td_a:		.ds	2
_fcr_first::	.ds	1
_fcr_n::	.ds	1

_sts_ptr::	.ds	2
_sts_n::	.ds	1
_sts_dx::	.ds	1
_sts_size::	.ds	2

	.area	_CODE
_tmr_draw::
	ld	a, (_tmr_tm)
	ld	l, a
	ld	h, #0
	add	hl, hl
	add	hl, hl
	ld	de, #_tilemaps
	add	hl, de
	ld	a, (hl)
	ld	(_tmr_w), a
	inc	hl
	inc	hl
	ld	e, (hl)
	inc	hl
	ld	d, (hl)
	push	de
	ld	hl, (_tmr_col)
	ld	de, (_tmr_lo)
	or	a, a
	sbc	hl, de
	jp	m, tw_lo
	ld	hl, (_tmr_col)
	jr	tw_a
tw_lo:
	ex	de, hl
tw_a:
	ld	(td_a), hl
	ld	a, (_tmr_w)
	ld	e, a
	ld	d, #0
	ld	hl, (_tmr_col)
	add	hl, de
	ld	de, (_tmr_hi)
	push	hl
	or	a, a
	sbc	hl, de
	pop	hl
	jp	m, tw_b
	ex	de, hl
tw_b:
	ld	de, (td_a)
	or	a, a
	sbc	hl, de
	jp	m, tw_none
	jr	z, tw_none
	ld	a, l
	ld	(_tmr_n), a
	ld	a, e
	and	a, #0x1F
	ld	(_tmr_c0), a
	ld	hl, (_tmr_col)
	ex	de, hl
	or	a, a
	sbc	hl, de
	ld	a, l
	ld	(_tmr_i0), a
	pop	hl
	ld	a, (_tmr_row)
	add	a, a
	ld	e, a
	ld	d, #0
	add	hl, de
	ld	e, (hl)
	inc	hl
	ld	d, (hl)
	ld	hl, #_tm_rows
	add	hl, de
	ld	de, (_tmr_last)
	or	a, a
	sbc	hl, de
	add	hl, de
	jp	z, _tmr_put
	ld	(_tmr_src), hl
	ld	a, (_tmr_opaque)
	or	a, a
	jr	z, tw_buf
	ld	a, (_tmr_i0)
	or	a, a
	jr	nz, tw_buf
	ld	a, (_tmr_w)
	ld	b, a
	ld	a, (_tmr_n)
	cp	a, b
	jr	nz, tw_buf
	ld	a, (_tmr_c0)
	add	a, b
	cp	a, #ROW_CELLS + 1
	jr	nc, tw_buf
	ld	hl, #0
	ld	(_tmr_last), hl
	ld	a, (_tmr_c0)
	add	a, a
	ld	e, a
	ld	d, #0
	ld	hl, (_tmr_dst)
	add	hl, de
	ex	de, hl
	ld	hl, (_tmr_src)
	ld	c, b
	jp	td_next
tw_buf:
	ld	hl, (_tmr_src)
	ld	(_tmr_last), hl
	call	_tmr_decode
	jp	_tmr_put
tw_none:
	pop	de
	ret

_tmr_decode::
	ld	hl, (_tmr_src)
	ld	de, #_tmr_buf
	ld	a, (_tmr_w)
	ld	c, a
td_next:
	ld	a, c
	or	a, a
	ret	z
	ld	a, (hl)
	inc	hl
	push	af
	and	a, #0x3F
	inc	a
	ld	b, a
	ld	a, c
	sub	a, b
	ld	c, a
	pop	af
	and	a, #0xC0
	jr	z, td_lit
	cp	a, #0x80
	jr	z, td_seq
	push	bc
	ld	a, (hl)
	ld	(de), a
	inc	hl
	inc	de
	ld	a, (hl)
	ld	(de), a
	inc	hl
	inc	de
	ld	a, b
	dec	a
	jr	z, td_rep_done
	add	a, a
	push	hl
	ld	h, d
	ld	l, e
	dec	hl
	dec	hl
	ld	c, a
	ld	b, #0
	ldir
	pop	hl
td_rep_done:
	pop	bc
	jr	td_next
td_seq:
	push	bc
	ld	a, b
	ld	c, (hl)
	inc	hl
	ld	b, (hl)
	inc	hl
	push	hl
	ex	de, hl
td_seq_loop:
	ld	(hl), c
	inc	hl
	ld	(hl), b
	inc	hl
	inc	bc
	dec	a
	jr	nz, td_seq_loop
	ex	de, hl
	pop	hl
	pop	bc
	jr	td_next
td_lit:
	push	bc
	ld	a, b
	add	a, a
	ld	c, a
	ld	b, #0
	ldir
	pop	bc
	jr	td_next

tp_setup:
	ld	a, (_tmr_c0)
	ld	c, a
	add	a, a
	ld	e, a
	ld	d, #0
	ld	hl, (_tmr_dst)
	add	hl, de
	ex	de, hl
	ld	a, #ROW_CELLS
	sub	a, c
	ld	c, a
	ld	a, (_tmr_n)
	ld	b, a
	ret

tp_wrap:
	push	hl
	ld	hl, #-2 * ROW_CELLS
	add	hl, de
	ex	de, hl
	pop	hl
	ld	c, #0xFF
	ret

_tmr_put::
	call	tp_setup
	ld	a, (_tmr_i0)
	add	a, a
	push	de
	ld	e, a
	ld	d, #0
	ld	hl, #_tmr_buf
	add	hl, de
	pop	de
	ld	a, (_tmr_opaque)
	or	a, a
	jr	z, tp_trans
tp_opq:
	ld	a, (hl)
	ld	(de), a
	inc	hl
	inc	de
	ld	a, (hl)
	ld	(de), a
	inc	hl
	inc	de
	dec	c
	call	z, tp_wrap
	djnz	tp_opq
	ret
tp_trans:
	ld	a, (hl)
	inc	hl
	or	a, (hl)
	jr	z, tp_skip
	dec	hl
	ld	a, (hl)
	ld	(de), a
	inc	hl
	inc	de
	ld	a, (hl)
	ld	(de), a
	inc	hl
	inc	de
	jr	tp_tnext
tp_skip:
	inc	hl
	inc	de
	inc	de
tp_tnext:
	dec	c
	call	z, tp_wrap
	djnz	tp_trans
	ret

_tmr_clear::
	call	tp_setup
	ld	a, b
	cp	a, c
	jr	c, tc_one
	ld	b, c
	sub	a, c
	push	af
	call	tc_run
	ld	hl, #-2 * ROW_CELLS
	add	hl, de
	ex	de, hl
	pop	af
	ret	z
	ld	b, a
	jr	tc_run
tc_one:
tc_run:
	ex	de, hl
	xor	a, a
tc_loop:
	ld	(hl), a
	inc	hl
	ld	(hl), a
	inc	hl
	djnz	tc_loop
	ex	de, hl
	ret

_fcr_fill::
	ld	a, (_fcr_first)
	ld	l, a
	ld	h, #0
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	ld	de, #FG_BASE
	add	hl, de
	ld	a, (_fcr_n)
	ld	c, a
	xor	a, a
fc_row:
	push	hl
	ld	b, #15
fc_cell:
	ld	(hl), a
	inc	l
	ld	(hl), a
	inc	l
	ld	(hl), a
	inc	l
	ld	(hl), a
	inc	l
	djnz	fc_cell
	pop	hl
	ld	de, #2 * ROW_CELLS
	add	hl, de
	dec	c
	jr	nz, fc_row
	ret

_st_shift::
	ld	a, (_sts_n)
	or	a, a
	ret	z
	ld	b, a
	ld	hl, (_sts_ptr)
	ld	de, (_sts_size)
	ld	a, (_sts_dx)
	ld	c, a
	bit	7, a
	jr	nz, ss_neg
ss_pos:
	ld	a, (hl)
	add	a, c
	ld	(hl), a
	jr	nc, ss_p_next
	ld	a, (de)
	xor	a, #0x10
	ld	(de), a
ss_p_next:
	inc	hl
	inc	hl
	inc	hl
	inc	hl
	inc	de
	djnz	ss_pos
	ret
ss_neg:
	ld	a, (hl)
	add	a, c
	ld	(hl), a
	jr	c, ss_n_next
	ld	a, (de)
	xor	a, #0x10
	ld	(de), a
ss_n_next:
	inc	hl
	inc	hl
	inc	hl
	inc	hl
	inc	de
	djnz	ss_neg
	ret
