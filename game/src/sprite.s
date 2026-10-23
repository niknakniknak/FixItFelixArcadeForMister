; SPDX-FileCopyrightText: 2026 Chris Watson
; SPDX-License-Identifier: GPL-3.0-or-later
	.module sprite
	.globl	_spr_put_parts
	.globl	_big_list
	.globl	_big_used
	.globl	_spr_out
	.globl	_spr_free
	.globl	_spr_part
	.globl	_spr_count
	.globl	_spr_x
	.globl	_spr_y
	.globl	_spr_add
	.globl	_spr_add_below
	.globl	_fg_pic
	.globl	_fgp_map
	.globl	_fgp_col
	.globl	_fgp_row
	.globl	_fgp_w
	.globl	_fgp_h
	.globl	_fgp_mode
	.globl	_tm_rows
	.globl	_actor_draw
	.globl	_actor_tick
	.globl	_actor_draw_super
	.globl	_frame_cell
	.globl	_spr_frame
	.globl	_anims
	.globl	_msprites
	.globl	_spr_parts
	.globl	_scroll_x
	.globl	_scroll_y

	Y8 = 0x04
	SCREEN_H_LO = 0x40
	FG_BASE = 0xE000
	FG_COLS = 30
	CREDIT_ROW = 39
	CREDIT_END = 12
	FG_END = 40
	MODE_SEE = 2
	AF_VISIBLE = 0x01
	AF_MIRROR = 0x04
	SCREEN_W = 240
	X_LEFT = 0xF1
	X8_BIT = 0x10
	SIZE_PAGE = 0xF2
	LIST_MAX = 96

	.area	_DATA
_spr_out::	.ds	2
_spr_free::	.ds	1
_spr_part::	.ds	2
_spr_count::	.ds	1
_spr_x::	.ds	2
_spr_y::	.ds	2
_spr_add::	.ds	1
_spr_add_below::	.ds	1
y8:		.ds	1
x8:		.ds	1
_fgp_map::	.ds	2
_fgp_col::	.ds	1
_fgp_row::	.ds	1
_fgp_w::	.ds	1
_fgp_h::	.ds	1
_fgp_mode::	.ds	1
fp_y:		.ds	1
fp_lo:		.ds	1
fp_dst:		.ds	2
fp_seen:	.ds	1
fp_last:	.ds	1
ad_f:		.ds	1
ad_h:		.ds	1
ad_flags:	.ds	1

	.area	_CODE
_actor_tick::
	ex	de, hl
	ld	hl, #3
	add	hl, de
	ld	a, (hl)
	and	a, #0xEF
	ld	(hl), a
	and	a, #0x0B
	dec	a
	ret	nz
	ld	a, (de)
	ld	l, a
	ld	h, #0
	add	hl, hl
	add	hl, hl
	ld	bc, #_anims
	add	hl, bc
	push	hl
	pop	iy
	ld	a, 3 (iy)
	or	a, a
	ret	z
	ld	hl, #2
	add	hl, de
	ld	a, (hl)
	or	a, a
	jr	z, at_step
	dec	(hl)
	ret
at_step:
	inc	hl
	set	4, (hl)
	dec	hl
	dec	hl
	ld	a, (hl)
	inc	a
	cp	a, 2 (iy)
	jr	c, at_set
	ld	a, 3 (iy)
	cp	a, #2
	jr	nz, at_wrap
	inc	hl
	inc	hl
	set	1, (hl)
	dec	hl
	dec	hl
	ld	a, 2 (iy)
	dec	a
	jr	at_set
at_wrap:
	ld	a, (hl)
	inc	a
	sub	a, 2 (iy)
at_set:
	ld	(hl), a
	inc	hl
	ex	de, hl
	ld	l, a
	ld	h, #0
	ld	b, h
	ld	c, l
	add	hl, hl
	add	hl, bc
	add	hl, hl
	add	hl, bc
	ld	c, 0 (iy)
	ld	b, 1 (iy)
	add	hl, bc
	inc	hl
	inc	hl
	ld	a, (hl)
	ld	(de), a
	ret

_actor_draw_super::
	ld	iy, #2
	add	iy, sp
	ld	a, 2 (iy)
	ld	(ad_f), a
	ld	a, 3 (iy)
	ld	(ad_h), a
	ld	c, 0 (iy)
	ld	b, 1 (iy)
	pop	iy
	pop	af
	pop	af
	push	iy
	jr	ads_common
_actor_draw::
	xor	a, a
	ld	(ad_f), a
	ld	(ad_h), a
	pop	iy
	pop	bc
	push	iy
ads_common:
	inc	hl
	inc	hl
	inc	hl
	ld	a, (hl)
	bit	0, a
	ret	z
	ld	(ad_flags), a
	dec	hl
	dec	hl
	ld	a, (hl)
	dec	hl
	ld	l, (hl)
	push	de
	push	bc
	push	af
	ld	h, #0
	add	hl, hl
	add	hl, hl
	ld	de, #_anims
	add	hl, de
	ld	e, (hl)
	inc	hl
	ld	d, (hl)
	pop	af
	ld	l, a
	ld	h, #0
	ld	b, h
	ld	c, l
	add	hl, hl
	add	hl, bc
	add	hl, hl
	add	hl, bc
	add	hl, de
	pop	bc
	pop	de
	push	hl
	ld	hl, (_scroll_x)
	add	hl, de
	ex	de, hl
	ld	hl, (_scroll_y)
	add	hl, bc
	ld	b, h
	ld	c, l
	pop	hl
	ld	a, (ad_flags)
	and	a, #AF_MIRROR
	ld	iy, #ad_f
	or	a, 0 (iy)
	or	a, 1 (iy)
	jr	z, fc_core
	ld	a, (ad_h)
	push	af
	inc	sp
	ld	a, (ad_f)
	push	af
	inc	sp
	ld	a, (ad_flags)
	and	a, #AF_MIRROR
	push	af
	inc	sp
	push	bc
	call	_spr_frame
	ret

_frame_cell::
	pop	iy
	pop	bc
	push	iy
fc_core:
	push	hl
	ld	a, (hl)
	ld	l, a
	ld	h, #0
	add	hl, hl
	push	de
	ld	d, h
	ld	e, l
	add	hl, hl
	add	hl, de
	ld	de, #_msprites
	add	hl, de
	pop	de
	inc	hl
	inc	hl
	ld	a, (hl)
	dec	a
	jr	nz, fc_multi
	inc	hl
	ld	a, (hl)
	or	a, a
	jr	nz, fc_multi
	dec	hl
	dec	hl
	ld	a, (hl)
	dec	hl
	ld	l, (hl)
	ld	h, a
	add	hl, hl
	add	hl, hl
	push	de
	ld	de, #_spr_parts
	add	hl, de
	pop	de
	ex	(sp), hl
	inc	hl
	inc	hl
	inc	hl
	ld	a, e
	sub	a, (hl)
	ld	e, a
	inc	hl
	ld	a, d
	sbc	a, (hl)
	ld	d, a
	inc	hl
	ld	a, c
	sub	a, (hl)
	ld	c, a
	inc	hl
	ld	a, b
	sbc	a, (hl)
	ld	b, a
	pop	hl
	ld	a, (_spr_free)
	or	a, a
	ret	z
	ld	a, e
	add	a, (hl)
	ld	e, a
	ld	a, d
	adc	a, #0
	ret	nz
	ld	a, e
	cp	a, #SCREEN_W
	ret	nc
	inc	hl
	ld	a, c
	add	a, (hl)
	ld	c, a
	ld	a, b
	adc	a, #0
	jr	z, fc_low
	dec	a
	ret	nz
	ld	a, c
	cp	a, #SCREEN_H_LO
	ret	nc
	ld	a, #Y8
fc_low:
	ld	b, a
	push	hl
	ld	hl, (_spr_out)
	ld	(hl), e
	inc	hl
	ld	(hl), c
	inc	hl
	ex	de, hl
	pop	hl
	inc	hl
	ld	a, (hl)
	ld	(de), a
	inc	de
	inc	hl
	ld	a, (hl)
	or	a, b
	ld	(de), a
	inc	de
	ld	(_spr_out), de
	ld	hl, #_spr_free
	dec	(hl)
	ret
fc_multi:
	pop	hl
	xor	a, a
	push	af
	inc	sp
	push	af
	inc	sp
	push	af
	inc	sp
	push	bc
	call	_spr_frame
	ret

_spr_put_parts::
	ld	a, (_spr_free)
	or	a, a
	ret	z
	ld	c, a
	ld	a, (_spr_count)
	or	a, a
	ret	z
	ld	b, a
	ld	hl, (_spr_part)
	ld	de, (_spr_out)
part:
	ld	a, (_spr_x)
	add	a, (hl)
	ld	(de), a
	ld	a, (_spr_x + 1)
	adc	a, #0
	jr	z, x_low
	inc	a
	jr	nz, skip_at_dx
	ld	a, (de)
	cp	a, #X_LEFT
	jr	c, skip_at_dx
	ld	a, #X8_BIT
x_low:
	ld	(x8), a
	inc	hl
	inc	de
	ld	a, (_spr_y)
	add	a, (hl)
	ld	(de), a
	ld	a, (_spr_y + 1)
	adc	a, #0
	jr	z, y_low
	dec	a
	jr	nz, skip_at_dy
	ld	a, (de)
	cp	a, #SCREEN_H_LO
	jr	nc, skip_at_dy
	ld	a, #Y8
y_low:
	ld	(y8), a
	inc	hl
	inc	de
	ld	a, (hl)
	ld	(de), a
	inc	hl
	inc	de
	ld	a, (_spr_add_below)
	cp	a, (hl)
	ld	a, (hl)
	jr	z, no_add
	jr	c, no_add
	ld	a, (_spr_add)
	add	a, (hl)
no_add:
	push	hl
	ld	hl, #y8
	or	a, (hl)
	pop	hl
	ld	(de), a
	ld	a, (x8)
	or	a, a
	call	nz, mark_x8
	inc	hl
	inc	de
	dec	c
	jr	z, done
	djnz	part
	jr	done

skip_at_dy:
	dec	de
	jr	skip_rest

skip_at_dx:
	inc	hl
skip_rest:
	inc	hl
	inc	hl
	inc	hl
	djnz	part

done:
	ld	(_spr_out), de
	ld	a, c
	ld	(_spr_free), a
	ret

mark_x8:
	push	hl
	push	bc
	ld	b, a
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
	ld	h, #SIZE_PAGE
	ld	(hl), b
	ld	a, (_big_used)
	cp	a, #LIST_MAX
	jr	nc, mx_done
	ld	c, a
	inc	a
	ld	(_big_used), a
	ld	b, #0
	ld	a, l
	ld	hl, #_big_list
	add	hl, bc
	ld	(hl), a
mx_done:
	pop	bc
	pop	hl
	ret

_fg_pic::
	xor	a, a
	ld	(fp_y), a
fp_rows:
	ld	a, (fp_y)
	ld	hl, #_fgp_h
	cp	a, (hl)
	ret	nc
	ld	hl, #_fgp_row
	add	a, (hl)
	cp	a, #FG_END
	ret	nc
	ld	l, a
	cp	a, #CREDIT_ROW
	ld	a, #0
	jr	nz, fp_lo1
	ld	a, #CREDIT_END
fp_lo1:
	ld	(fp_lo), a
	ld	h, #0
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	ld	de, #FG_BASE
	add	hl, de
	ld	(fp_dst), hl
	ld	a, (fp_y)
	ld	l, a
	ld	h, #0
	add	hl, hl
	ld	de, (_fgp_map)
	add	hl, de
	ld	a, (hl)
	inc	hl
	ld	h, (hl)
	ld	l, a
	ld	de, #_tm_rows
	add	hl, de
	call	fr_row
	ld	hl, #fp_y
	inc	(hl)
	jr	fp_rows

fr_row:
	push	hl
	ld	a, (fp_lo)
	ld	h, a
	neg
	add	a, #FG_COLS
	ld	l, a
	ld	a, (_fgp_w)
	ld	d, a
	ld	a, (_fgp_mode)
	ld	e, a
	ld	a, (_fgp_col)
	ld	c, a
	exx
	ld	l, a
	rla
	sbc	a, a
	ld	h, a
	add	hl, hl
	ld	de, (fp_dst)
	add	hl, de
	ex	de, hl
	pop	hl
	xor	a, a
	ld	(fp_seen), a
	ld	(fp_last), a
fr_op:
	ld	a, (hl)
	inc	hl
	ld	b, a
	and	a, #0x3F
	inc	a
	exx
	ld	b, a
	exx
	ld	a, b
	and	a, #0xC0
	jr	z, fr_first
	ld	c, (hl)
	inc	hl
	ld	b, (hl)
	inc	hl
	cp	a, #0x80
	jr	z, fr_seqf
	ld	a, b
	or	a, c
	jr	z, fr_zero
	call	fr_begin
fr_rep:
	call	fr_put
	jr	nz, fr_rep
	jr	fr_op
fr_seqf:
	call	fr_begin
fr_seq:
	call	fr_put
	inc	bc
	jr	nz, fr_seq
	jr	fr_op
fr_first:
	call	fr_begin
fr_lit:
	ld	c, (hl)
	inc	hl
	ld	b, (hl)
	inc	hl
	call	fr_put
	jr	nz, fr_lit
	jr	fr_op
fr_zero:
	xor	a, a
	ld	(fp_last), a
	ld	a, (_fgp_mode)
	cp	a, #MODE_SEE
	jr	z, fr_skipz
	exx
	ld	a, d
	sub	a, b
	exx
	jr	nz, fr_z1
	ld	a, (fp_seen)
	or	a, a
	ret	z
	exx
	ld	b, #1
	exx
	call	fr_put
	ret
fr_z1:
	ld	a, (fp_seen)
	or	a, a
	jr	nz, fr_rep
	exx
	ld	a, b
	dec	a
	push	af
	add	a, c
	ld	c, a
	pop	af
	push	af
	neg
	add	a, d
	ld	d, a
	ld	b, #1
	pop	af
	exx
	add	a, a
	add	a, e
	ld	e, a
	adc	a, d
	sub	a, e
	ld	d, a
	jr	fr_rep

fr_skipz:
	exx
	ld	a, c
	add	a, b
	ld	c, a
	ld	a, d
	sub	a, b
	ld	d, a
	ld	a, b
	exx
	ret	z
	ret	c
	add	a, a
	add	a, e
	ld	e, a
	adc	a, d
	sub	a, e
	ld	d, a
	jp	fr_op

fr_begin:
	ld	a, (_fgp_mode)
	cp	a, #MODE_SEE
	ret	z
	ld	a, #1
	ld	(fp_last), a
	ld	a, (fp_seen)
	or	a, a
	ret	nz
	inc	a
	ld	(fp_seen), a
	exx
	ld	a, (_fgp_col)
	cp	a, c
	exx
	ret	nz
	dec	de
	dec	de
	exx
	dec	c
	ld	a, c
	sub	a, h
	cp	a, l
	inc	c
	exx
	jr	nc, fr_b1
	xor	a, a
	ld	(de), a
	inc	de
	ld	(de), a
	inc	de
	ret
fr_b1:
	inc	de
	inc	de
	ret

fr_put:
	exx
	ld	a, c
	sub	a, h
	cp	a, l
	ld	a, e
	exx
	jr	nc, fr_skip
	cp	a, #MODE_SEE
	jr	nz, fr_w
	ld	a, b
	or	a, c
	jr	z, fr_skip
	xor	a, a
fr_w:
	ex	de, hl
	or	a, a
	jr	nz, fr_clr
	ld	(hl), c
	inc	hl
	ld	(hl), b
	jr	fr_next
fr_clr:
	xor	a, a
	ld	(hl), a
	inc	hl
	ld	(hl), a
fr_next:
	inc	hl
	ex	de, hl
	jr	fr_count
fr_skip:
	inc	de
	inc	de
fr_count:
	exx
	inc	c
	dec	d
	jr	z, fr_end
	dec	b
	exx
	ret
fr_end:
	exx
	pop	af
	ld	a, (fp_last)
	or	a, a
	ret	z
	exx
	ld	a, c
	sub	a, h
	cp	a, l
	exx
	ret	nc
	xor	a, a
	ld	(de), a
	inc	de
	ld	(de), a
	ret
