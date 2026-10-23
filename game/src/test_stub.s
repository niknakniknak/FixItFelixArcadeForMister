; SPDX-FileCopyrightText: 2026 Chris Watson
; SPDX-License-Identifier: GPL-3.0-or-later
	.module test_stub
	.globl	_tm_rows
	.globl	_scroll_x
	.globl	_scroll_y

	.area	_CODE
_tm_rows::

	.area	_DATA
_scroll_x::
	.ds	2
_scroll_y::
	.ds	2
