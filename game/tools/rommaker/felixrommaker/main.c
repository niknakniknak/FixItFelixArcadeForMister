/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "fr.h"
#include "embed.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vendor/miniz.h"

#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#define STB_IMAGE_IMPLEMENTATION
#include "vendor/stb_image.h"

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#else
#include <dirent.h>
#include <unistd.h>
#endif

typedef struct {
    const char *name, *md5;
} known_t;

static const known_t windows_md5[] = {
    { "Data/Controls.log", "96a30e8f61235a06eb18fd9da8b7fc79" },
    { "Data/felix1.png", "f5cf8b74c8d131f429456ea73ddaa651" },
    { "Data/felix2.png", "237a0df0c1c0401c1dbfe0df480abb76" },
    { "Data/felix3.png", "40d5262ef1dec4394a2b7e1a340c37e6" },
    { "Data/felix4.png", "01e5d542077432923d855cb7cb154675" },
    { "Data/hammer0.png", "470321401a0cfa3e41e725b8bdc0816a" },
    { "Data/hammer1.png", "23441d0434b1f8f103c7f9525e5eaf20" },
    { "Data/hammer2.png", "1464db2f434b9bb7792459339a620109" },
    { "Data/hammer3.png", "b6cb11e2a715caf36256e21a81966a6c" },
    { "Data/sound0.wav", "10b3936acfc41f8208a74c9b4c7b7b23" },
    { "Data/sound1.wav", "a3e0a0279385296eed6a2317e4eb3459" },
    { "Data/sound10.wav", "32f2bfec52b9daa88afe29b84b8a8069" },
    { "Data/sound11.wav", "bac4005a8fb20b1ebb02ab8e9ea4d718" },
    { "Data/sound12.wav", "2c2cf477300c98f5ea16eb53bd1dc693" },
    { "Data/sound13.wav", "a7645ad32cba5f458a2fc3ea867e5059" },
    { "Data/sound14.wav", "23a015d39d52e44aebf7979ddb670468" },
    { "Data/sound15.wav", "48c7e5d16b77559e26eeaddeb87438c5" },
    { "Data/sound16.wav", "50fa2dc5dc74bfa91759e2c64f9c71c4" },
    { "Data/sound17.wav", "74bd9873202a12d4d4c1ad51342044cc" },
    { "Data/sound18.wav", "966559ffca7baa77f4aa1fb796ecb5ba" },
    { "Data/sound19.wav", "d5cb21668642447ab78024083c48e69d" },
    { "Data/sound2.wav", "1bd14a6b321cf3097c790150efef3adc" },
    { "Data/sound20.wav", "07fa0e310144bea1c2c9ffc5a10297cc" },
    { "Data/sound21.wav", "1d4edcee5bf56ddbd6a028f499bc6e2d" },
    { "Data/sound22.wav", "d4acf74466587a82bb233fec59153531" },
    { "Data/sound23.wav", "dc455f888d2ac80ea945ea6c256ddd2b" },
    { "Data/sound24.wav", "02533080b2435fae52fad115ac0076f1" },
    { "Data/sound25.wav", "578f4890257a780ce9672c6c3855021e" },
    { "Data/sound26.wav", "6cb47377dbe6cb7641728594155b193b" },
    { "Data/sound27.wav", "46821cc98ef458b57f04e02c7a1e47f9" },
    { "Data/sound28.wav", "6d87993a12ad28bdf990343018b19f9b" },
    { "Data/sound29.wav", "0571b09853944a47395f8bb9a278f2f7" },
    { "Data/sound3.wav", "bd567e61939a1f8b4166ac71685025f2" },
    { "Data/sound4.wav", "49b254072510d8c4c5cd52c5a7f135e1" },
    { "Data/sound5.wav", "7ebc0ef4a13abf0adab0478d326ae6ac" },
    { "Data/sound6.wav", "f0be968efe94c2b464cb6f6b27e007e6" },
    { "Data/sound7.wav", "6f1033478b665ce8ba71920eedab0e79" },
    { "Data/sound8.wav", "114ce7fc2bfb9c8c122d2793cbdc66fb" },
    { "Data/sound9.wav", "87e0e8355d1190fe783e0b1ef3563df7" },
    { "Data/texture2.bmp", "32ec1ee5d870fedc3f1a6cb0492e8f93" },
    { "FixitFelixJr.exe", "2bfc8fac7af8ddac15159f9a82b0f1e1" },
    { 0, 0 },
};

static const known_t atgames_md5[] = {
    { "sound30.wav", "2fed29063b33a80fbbb3b7bbaec2e1b3" },
    { "sound31.wav", "3a572ed2cc464935abb172de7f18ddbe" },
    { 0, 0 },
};

#define EXE_MD5 "2bfc8fac7af8ddac15159f9a82b0f1e1"
#define MEGADRIVE_MD5 "38a3555768b31dc2abfc34d20ef546ca"

typedef struct {
    const char *region;
    int size, n;
    const char *names[8];
} chip_t;

static const chip_t chips[] = {
    { "main", 0x8000, 4, { "fj-p1.1d", "fj-p2.1e", "fj-p3.1f", "fj-p4.1g" } },
    { "tiles", 0x8000, 2, { "fj-t1.5h", "fj-t2.5j" } },
    { "sprites", 0x8000, 8, { "fj-s1.7a", "fj-s2.7b", "fj-s3.7c", "fj-s4.7d", "fj-s5.7e", "fj-s6.7f", "fj-s7.7g", "fj-s8.7h" } },
    { "samples", 0x80000, 4, { "fj-v1.3a", "fj-v2.3b", "fj-v3.3c", "fj-v4.3d" } },
    { "pal", 0x400, 2, { "fj-pl.6b", "fj-ph.6c" } },
};

typedef struct {
    char *exe, *atgames, *megadrive, *output;
} options_t;

typedef struct {
    char *text;
    size_t len;
} msg_t;

static void msg_add(msg_t *m, const char *s)
{
    size_t n = strlen(s);
    m->text = xrealloc(m->text, m->len + n + 2);
    if (m->len)
        m->text[m->len++] = '\n';
    memcpy(m->text + m->len, s, n + 1);
    m->len += n;
}

static char *native_name(const char *name)
{
    char *s = xstrdup(name), *p;
#ifdef _WIN32
    for (p = s; *p; p++)
        if (*p == '/')
            *p = '\\';
#else
    (void)p;
#endif
    return s;
}

static void check_files(const char *root, const known_t *table, const char *label, msg_t *problems)
{
    int i;
    for (i = 0; table[i].name; i++) {
        buf_t b;
        char md5[33], *path = path_join(root, native_name(table[i].name));
        if (!read_file(path, &b)) {
            msg_add(problems, strf("%s: missing %s", label, table[i].name));
            continue;
        }
        md5_hex(b.data, b.len, md5);
        if (strcmp(md5, table[i].md5))
            msg_add(problems, strf("%s: %s is not the known good file (md5 %s)", label, table[i].name, md5));
        free(b.data);
    }
}

static int ieq(const char *a, const char *b)
{
    for (; *a && *b; a++, b++) {
        char x = *a >= 'A' && *a <= 'Z' ? (char)(*a + 32) : *a;
        char y = *b >= 'A' && *b <= 'Z' ? (char)(*b + 32) : *b;
        if (x != y)
            return 0;
    }
    return *a == *b;
}

static int exists(const char *path)
{
    long long size;
    return file_size(path, &size);
}

static void classify(const char *path, options_t *o)
{
    long long size;
    buf_t b;
    char md5[33];
    if (!file_size(path, &size))
        return;
    if (is_dir(path)) {
        char *p = path_join(path, "sound30.wav");
        if (exists(p) && !o->atgames) {
            o->atgames = xstrdup(path);
            return;
        }
        p = path_join(path_join(path, "res_fif"), "sound30.wav");
        if (exists(p) && !o->atgames)
            o->atgames = path_join(path, "res_fif");
        p = path_join(path, "FixitFelixJr.exe");
        if (exists(p) && !o->exe)
            o->exe = p;
        return;
    }
    if (ieq(path_base(path), "fif_libretro.so")) {
        classify(path_join(path_dir(path), "res_fif"), o);
        return;
    }
    if (size > (8 << 20))
        return;
    if (!read_file(path, &b))
        return;
    md5_hex(b.data, b.len, md5);
    free(b.data);
    if (!strcmp(md5, EXE_MD5) && !o->exe)
        o->exe = xstrdup(path);
    else if (!strcmp(md5, MEGADRIVE_MD5) && !o->megadrive)
        o->megadrive = xstrdup(path);
}

static void look_entry(const char *dir, const char *name, int isdir, options_t *o)
{
    if (isdir && ieq(name, "res_fif"))
        classify(path_join(dir, name), o);
    if (!isdir && !ieq(name, "felixrommaker.exe") && !ieq(name, "felixjr.zip"))
        classify(path_join(dir, name), o);
}

#ifdef _WIN32
static char *utf8(const wchar_t *w)
{
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
    char *s = xmalloc((size_t)n);
    WideCharToMultiByte(CP_UTF8, 0, w, -1, s, n, NULL, NULL);
    return s;
}

static void look_around(const char *dir, options_t *o)
{
    WIN32_FIND_DATAW fd;
    HANDLE h;
    char *pattern = path_join(dir, "*");
    int n = MultiByteToWideChar(CP_UTF8, 0, pattern, -1, NULL, 0);
    wchar_t *w = xmalloc(sizeof(wchar_t) * (size_t)n);
    char **names = NULL;
    int *dirs = NULL, count = 0, i;
    classify(dir, o);
    MultiByteToWideChar(CP_UTF8, 0, pattern, -1, w, n);
    h = FindFirstFileW(w, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return;
    do {
        char *name = utf8(fd.cFileName);
        if (!strcmp(name, ".") || !strcmp(name, ".."))
            continue;
        names = xrealloc(names, sizeof(char *) * (size_t)(count + 1));
        dirs = xrealloc(dirs, sizeof(int) * (size_t)(count + 1));
        names[count] = name;
        dirs[count++] = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    for (i = 1; i < count; i++) {
        int j = i;
        while (j > 0 && strcmp(names[j - 1], names[j]) > 0) {
            char *t = names[j];
            int d = dirs[j];
            names[j] = names[j - 1];
            dirs[j] = dirs[j - 1];
            names[j - 1] = t;
            dirs[j - 1] = d;
            j--;
        }
    }
    for (i = 0; i < count; i++)
        look_entry(dir, names[i], dirs[i], o);
}

static char *exe_dir(void)
{
    wchar_t w[32768];
    DWORD n = GetModuleFileNameW(NULL, w, 32768);
    if (!n || n >= 32768)
        return xstrdup(".");
    return path_dir(utf8(w));
}

static char *work_dir(void)
{
    wchar_t w[32768];
    DWORD n = GetCurrentDirectoryW(32768, w);
    if (!n || n >= 32768)
        return NULL;
    return utf8(w);
}
#else
static int name_cmp(const void *a, const void *b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

static void look_around(const char *dir, options_t *o)
{
    DIR *d;
    struct dirent *e;
    char **names = NULL;
    int count = 0, i;
    classify(dir, o);
    d = opendir(dir);
    if (!d)
        return;
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
            continue;
        names = xrealloc(names, sizeof(char *) * (size_t)(count + 1));
        names[count++] = xstrdup(e->d_name);
    }
    closedir(d);
    qsort(names, (size_t)count, sizeof(char *), name_cmp);
    for (i = 0; i < count; i++) {
        char *p = path_join(dir, names[i]);
        look_entry(dir, names[i], is_dir(p), o);
    }
}

static char *exe_dir(void)
{
    char buf[4096];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof buf - 1);
    if (n <= 0)
        return xstrdup(".");
    buf[n] = 0;
    return path_dir(buf);
}

static char *work_dir(void)
{
    char buf[4096];
    if (!getcwd(buf, sizeof buf))
        return NULL;
    return xstrdup(buf);
}
#endif

static int png_pixel(const char *path, int x, int y, int out[3], char *err, size_t errlen)
{
    buf_t b;
    int w, h, n;
    unsigned char *pix;
    if (!read_file(path, &b)) {
        snprintf(err, errlen, "open %s: file not found", path);
        return 0;
    }
    pix = stbi_load_from_memory(b.data, (int)b.len, &w, &h, &n, 4);
    free(b.data);
    if (!pix || x >= w || y >= h) {
        snprintf(err, errlen, "%s is not 8-bit RGBA", path);
        return 0;
    }
    out[0] = pix[((size_t)y * (size_t)w + (size_t)x) * 4];
    out[1] = pix[((size_t)y * (size_t)w + (size_t)x) * 4 + 1];
    out[2] = pix[((size_t)y * (size_t)w + (size_t)x) * 4 + 2];
    stbi_image_free(pix);
    return 1;
}

static void le16(buf_t *b, unsigned v)
{
    buf_u16(b, (int)v);
}

static void le32(buf_t *b, unsigned long v)
{
    buf_u16(b, (int)(v & 0xFFFF));
    buf_u16(b, (int)(v >> 16));
}

static int write_zip(const char *path, const char **names, uint8_t **data, const size_t *sizes, int n)
{
    buf_t zip = { 0 }, dir = { 0 };
    unsigned long *offsets = xmalloc(sizeof(unsigned long) * (size_t)n);
    int flags = (int)tdefl_create_comp_flags_from_zip_params(MZ_BEST_COMPRESSION, -15, MZ_DEFAULT_STRATEGY), i;
    FILE *f;
    for (i = 0; i < n; i++) {
        size_t clen = 0, nlen = strlen(names[i]);
        void *comp = tdefl_compress_mem_to_heap(data[i], sizes[i], &clen, flags);
        unsigned long crc = (unsigned long)mz_crc32(MZ_CRC32_INIT, data[i], sizes[i]);
        if (!comp)
            return 0;
        offsets[i] = (unsigned long)zip.len;
        le32(&zip, 0x04034b50);
        le16(&zip, 20);
        le16(&zip, 0);
        le16(&zip, 8);
        le16(&zip, 0);
        le16(&zip, 0x21);
        le32(&zip, crc);
        le32(&zip, (unsigned long)clen);
        le32(&zip, (unsigned long)sizes[i]);
        le16(&zip, (unsigned)nlen);
        le16(&zip, 0);
        buf_put(&zip, names[i], nlen);
        buf_put(&zip, comp, clen);
        mz_free(comp);
        le32(&dir, 0x02014b50);
        le16(&dir, 20);
        le16(&dir, 20);
        le16(&dir, 0);
        le16(&dir, 8);
        le16(&dir, 0);
        le16(&dir, 0x21);
        le32(&dir, crc);
        le32(&dir, (unsigned long)clen);
        le32(&dir, (unsigned long)sizes[i]);
        le16(&dir, (unsigned)nlen);
        le16(&dir, 0);
        le16(&dir, 0);
        le16(&dir, 0);
        le16(&dir, 0);
        le32(&dir, 0);
        le32(&dir, offsets[i]);
        buf_put(&dir, names[i], nlen);
    }
    {
        unsigned long at = (unsigned long)zip.len;
        buf_put(&zip, dir.data, dir.len);
        le32(&zip, 0x06054b50);
        le16(&zip, 0);
        le16(&zip, 0);
        le16(&zip, (unsigned)n);
        le16(&zip, (unsigned)n);
        le32(&zip, (unsigned long)dir.len);
        le32(&zip, at);
        le16(&zip, 0);
    }
    f = fr_open_write(path);
    if (!f)
        return 0;
    if (fwrite(zip.data, 1, zip.len, f) != zip.len) {
        fclose(f);
        return 0;
    }
    return fclose(f) == 0;
}

static int run(options_t *o, char *err, size_t errlen)
{
    msg_t problems = { 0 };
    char *root, *data, *variant;
    sound_in_t in;
    buf_t exe, bmp, log, prog, gfxblob, demo, samples;
    pe_t pe;
    static tables_t t;
    static gfx_t g;
    sheet_t s;
    int hammer[4][3], hx, hy, yx, yy, cx, cy, i, c, k, nfiles = 0;
    felix_variant_t felix[4];
    uint8_t *main_rom, *files[20];
    const char *names[20];
    size_t sizes[20];
    md5_ctx all;
    uint8_t digest[16];
    char sum[33], sha1hex[41], *path;
    const char *verdict = "no reference for this combination";
    int found = 0;
    if (!o->exe) {
        snprintf(err, errlen, "FixitFelixJr.exe not found: drop it (or its folder) onto felixrommaker.exe, or use --exe <path>");
        return 0;
    }
    memset(&in, 0, sizeof in);
    root = path_dir(o->exe);
    check_files(root, windows_md5, "Windows build", &problems);
    data = path_join(root, "Data");
    in.win = data;
    variant = xstrdup("windows");
    if (o->atgames) {
        check_files(o->atgames, atgames_md5, "AtGames res_fif", &problems);
        in.atg = o->atgames;
        variant = strf("%s + atgames", variant);
    }
    if (o->megadrive) {
        buf_t md;
        char md5[33];
        if (!read_file(o->megadrive, &md)) {
            snprintf(err, errlen, "open %s: file not found", o->megadrive);
            return 0;
        }
        md5_hex(md.data, md.len, md5);
        if (strcmp(md5, MEGADRIVE_MD5))
            msg_add(&problems, strf("Mega Drive ROM %s is not the known good file (md5 %s)", o->megadrive, md5));
        in.md = md.data;
        in.mdlen = md.len;
        variant = strf("%s + megadrive", variant);
    }
    if (problems.len) {
        snprintf(err, errlen, "%s", problems.text);
        return 0;
    }
    printf("Fix-It Felix Jr. ROM maker: %s\n", variant);
    if (!read_file(o->exe, &exe) || !pe_open(&pe, exe.data, exe.len)) {
        snprintf(err, errlen, "cannot read %s", o->exe);
        return 0;
    }
    read_tables(&pe, &t);
    gfx_exe(&pe);
    if (!read_file(path_join(data, "texture2.bmp"), &bmp) || !read_bmp(&bmp, &s)) {
        snprintf(err, errlen, "texture2.bmp is not an 8-bit BMP file");
        return 0;
    }
    sheet_first(&s, gfx_index("hammer", 0), &hx, &hy);
    for (i = 0; i < 4; i++) {
        char name[16];
        sprintf(name, "hammer%d.png", i);
        if (!png_pixel(path_join(data, name), hx, hy, hammer[i], err, errlen))
            return 0;
    }
    sheet_first(&s, gfx_index("clothing", 0), &yx, &yy);
    sheet_first(&s, gfx_index("clothing", 1), &cx, &cy);
    for (i = 0; i < 4; i++) {
        char name[16];
        sprintf(name, "felix%d.png", i + 1);
        path = path_join(data, name);
        if (!png_pixel(path, yx, yy, felix[i].yellow, err, errlen) || !png_pixel(path, cx, cy, felix[i].clothes, err, errlen))
            return 0;
    }
    printf("  graphics\n");
    fflush(stdout);
    build_gfx(&s, &t, hammer, felix, &g);
    {
        int n = 0;
        prog.data = (uint8_t *)stbi_zlib_decode_malloc((const char *)embed_prog_z, (int)embed_prog_zlen, &n);
        if (!prog.data || n != (int)embed_prog_len) {
            snprintf(err, errlen, "the built-in program is damaged");
            return 0;
        }
        prog.len = (size_t)n;
    }
    if (!fill_exe(prog.data, exe.data, exe.len, err, errlen))
        return 0;
    if (!read_file(path_join(data, "Controls.log"), &log)) {
        snprintf(err, errlen, "cannot read Controls.log");
        return 0;
    }
    demo_table(&log, &demo);
    for (i = 0; i < embed_nholes; i++) {
        const ehole_t *h = &embed_holes[i];
        buf_t *blob = &demo;
        if (!strcmp(h->kind, "gfx")) {
            gfx_tables(&t, &g, h->cpu, &gfxblob);
            blob = &gfxblob;
        } else if (strcmp(h->kind, "demo")) {
            snprintf(err, errlen, "no tables for %s", h->kind);
            return 0;
        }
        if ((int)blob->len != h->length) {
            snprintf(err, errlen, "%s tables are %d bytes, the program expects %d", h->kind, (int)blob->len, h->length);
            return 0;
        }
        memcpy(prog.data + h->offset, blob->data, blob->len);
    }
    for (k = 0; k < 0x20000; k += 0x8000) {
        int sum8 = 0;
        for (i = k; i < k + 0x7FFF; i++)
            sum8 += prog.data[i];
        prog.data[k + 0x7FFF] = (uint8_t)(-sum8);
    }
    main_rom = prog.data;
    printf("  samples\n");
    fflush(stdout);
    sample_rom(&in, &samples, sha1hex);
    md5_init(&all);
    for (c = 0; c < 5; c++) {
        const chip_t *ch = &chips[c];
        size_t total = (size_t)ch->size * (size_t)ch->n, have;
        const uint8_t *src;
        uint8_t fill, *blob;
        if (!strcmp(ch->region, "main")) {
            src = main_rom;
            have = 0x20000;
            fill = 0xFF;
        } else if (!strcmp(ch->region, "tiles")) {
            src = g.tiles.data;
            have = g.tiles.len;
            fill = 0;
        } else if (!strcmp(ch->region, "sprites")) {
            src = g.sprites.data;
            have = g.sprites.len;
            fill = 0;
        } else if (!strcmp(ch->region, "samples")) {
            src = samples.data;
            have = samples.len;
            fill = 0xFF;
        } else {
            src = g.pal.data;
            have = g.pal.len;
            fill = 0;
        }
        if (have > total) {
            snprintf(err, errlen, "%s is %#x bytes", ch->region, (unsigned)have);
            return 0;
        }
        blob = xmalloc(total);
        memcpy(blob, src, have);
        memset(blob + have, fill, total - have);
        for (k = 0; k < ch->n; k++) {
            names[nfiles] = ch->names[k];
            files[nfiles] = blob + (size_t)k * (size_t)ch->size;
            sizes[nfiles] = (size_t)ch->size;
            md5_update(&all, files[nfiles], (size_t)ch->size);
            nfiles++;
        }
    }
    md5_final(&all, digest);
    for (i = 0; i < 16; i++)
        sprintf(sum + 2 * i, "%02x", digest[i]);
    {
        char *key = xstrdup(variant), *p;
        for (p = key; *p; p++)
            if (*p == ' ')
                memmove(p, p + 1, strlen(p));
        for (i = 0; i < embed_nexpected; i++)
            if (!strcmp(embed_expected[i].variant, key)) {
                found = 1;
                if (strcmp(embed_expected[i].md5, sum)) {
                    snprintf(err, errlen, "the ROM set came out different from the release (md5 %s, expected %s); nothing written",
                             sum, embed_expected[i].md5);
                    return 0;
                }
                verdict = "matches the release";
            }
    }
    if (!write_zip(o->output, names, files, sizes, nfiles)) {
        snprintf(err, errlen, "cannot write %s", o->output);
        return 0;
    }
    for (i = 0; i < nfiles; i++)
        printf("  %-10s crc32 %08lx\n", names[i], (unsigned long)mz_crc32(MZ_CRC32_INIT, files[i], sizes[i]));
    (void)found;
    printf("wrote %s\nassembled md5 %s (%s), resampler coefficients sha1 %s\n", o->output, sum, verdict, sha1hex);
    return 1;
}

static int take_flag(const char *arg, const char *name, char **value, int *i, int argc, char **argv)
{
    const char *a = arg;
    size_t n = strlen(name);
    if (a[0] != '-')
        return 0;
    a++;
    if (a[0] == '-')
        a++;
    if (strncmp(a, name, n))
        return 0;
    if (a[n] == '=') {
        *value = xstrdup(a + n + 1);
        return 1;
    }
    if (a[n])
        return 0;
    if (*i + 1 >= argc)
        die("flag needs an argument: -%s", name);
    *value = xstrdup(argv[++*i]);
    return 1;
}

int main(int argc, char **argv)
{
    options_t o;
    char err[4096], *home, *wd;
    int i, nflags = 0, ok;
    char **args = argv;
#ifdef _WIN32
    {
        int wn;
        wchar_t **w = CommandLineToArgvW(GetCommandLineW(), &wn);
        args = xmalloc(sizeof(char *) * (size_t)(wn + 1));
        for (i = 0; i < wn; i++)
            args[i] = utf8(w[i]);
        args[wn] = NULL;
        argc = wn;
    }
#endif
    memset(&o, 0, sizeof o);
    for (i = 1; i < argc; i++) {
        if (!strcmp(args[i], "--")) {
            i++;
            break;
        }
        if (args[i][0] != '-' || !args[i][1])
            break;
        if (take_flag(args[i], "exe", &o.exe, &i, argc, args) || take_flag(args[i], "megadrive", &o.megadrive, &i, argc, args) ||
            take_flag(args[i], "atgames", &o.atgames, &i, argc, args) || take_flag(args[i], "o", &o.output, &i, argc, args)) {
            nflags++;
            continue;
        }
        fprintf(stderr, "flag provided but not defined: %s\n", args[i]);
        fprintf(stderr, "Usage: felixrommaker [--exe FixitFelixJr.exe] [--megadrive ROM] [--atgames res_fif] [-o felixjr.zip] [files...]\n");
        return 2;
    }
    home = exe_dir();
    for (; i < argc; i++)
        classify(args[i], &o);
    if (!o.exe) {
        look_around(home, &o);
        wd = work_dir();
        if (wd)
            look_around(wd, &o);
        if (o.exe)
            look_around(path_dir(o.exe), &o);
    }
    if (!o.output)
        o.output = path_join(home, "felixjr.zip");
    err[0] = 0;
    ok = run(&o, err, sizeof err);
    if (!ok)
        fprintf(stderr, "error: %s\n", err);
#ifdef _WIN32
    if (!nflags) {
        char line[16];
        printf("\nPress Enter to close.");
        fflush(stdout);
        if (!fgets(line, sizeof line, stdin))
            line[0] = 0;
    }
#endif
    return ok ? 0 : 1;
}
