/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef EMBED_H
#define EMBED_H

#include <stdint.h>

enum { K_DATA, K_IMM, K_CALC };
enum { T_NONE, T_U8, T_I8, T_U16, T_I16, T_U32, T_I32, T_F32, T_F64, T_STR };
enum { A_NONE, A_INT, A_FLOAT, A_LIST };
enum {
    X_FIX16, X_FIX16UP, X_SQRT, X_FIX8, X_ROUND, X_TRUNC, X_ADD, X_SUB, X_MUL, X_DIV, X_NEG, X_AND, X_SHR,
    X_PICK, X_SLICE, X_EVERY, X_REVERSE, X_MAGICDIV, X_STRIP0, X_LOWBIT, X_LOG2, X_ODDPART, X_INV16, X_RFLOORDIV
};
enum { C_NONE, C_CONCAT, C_SUM, C_DIFF, C_VADD, C_QUOT, C_PRODUCT, C_ZIP, C_VMIN, C_VMAX };

typedef struct {
    double f;
    int32_t i;
    uint16_t list;
    uint8_t op, argkind, n;
} xf_t;

typedef struct {
    uint32_t va;
    int16_t off, count, stride;
    uint16_t xf, from;
    uint8_t kind, type, calc, nxf, nfrom;
} record_t;

typedef struct {
    int32_t idx;
    int16_t rec, str;
    uint16_t idxs, nidx;
    int8_t idxkind, word;
} part_t;

typedef struct {
    uint16_t fields, parts, nparts;
    uint8_t section, nfields;
} etable_t;

typedef struct {
    uint8_t section;
    int32_t offset, length;
} esection_t;

typedef struct {
    int32_t offset;
    uint8_t kind;
    int16_t scalar;
    int32_t addend;
} reloc_t;

typedef struct {
    const char *kind;
    int32_t offset, cpu, length;
} ehole_t;

typedef struct {
    const char *variant, *md5;
} expected_t;

extern const uint8_t embed_prog_z[];
extern const uint32_t embed_prog_zlen, embed_prog_len;
extern const xf_t embed_xf[];
extern const int16_t embed_from[];
extern const int32_t embed_ints[];
extern const uint8_t embed_fields[];
extern const part_t embed_parts[];
extern const record_t embed_records[];
extern const char *const embed_record_names[];
extern const int embed_nrecords;
extern const part_t embed_scalars[];
extern const int embed_nscalars;
extern const etable_t embed_tables[];
extern const char *const embed_table_names[];
extern const int embed_ntables;
extern const esection_t embed_sections[];
extern const int embed_nsections;
extern const reloc_t embed_relocs[];
extern const int embed_nrelocs;
extern const ehole_t embed_holes[];
extern const int embed_nholes;
extern const expected_t embed_expected[];
extern const int embed_nexpected;

#endif
