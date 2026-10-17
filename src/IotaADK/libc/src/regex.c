/*
* IotaOS - regex.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include <regex.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

enum { OP_CHAR, OP_ANY, OP_CLASS, OP_SPLIT, OP_JMP, OP_BOL, OP_EOL, OP_MATCH };

typedef struct {
    int op;
    int arg;     /* character, or class index */
    int x, y;    /* jump targets */
} inst_t;

typedef struct {
    inst_t *code;
    int len, cap;
    unsigned char (*classes)[32];
    int nclasses, class_cap;
    int icase;
} prog_t;

typedef struct {
    const char *p;
    int extended;
    int error;
    prog_t *prog;
} parser_t;

/* ---- program building ---- */

static int emit(prog_t *g, int op, int arg) {
    if (g->len == g->cap) {
        g->cap = g->cap ? g->cap * 2 : 32;
        g->code = realloc(g->code, sizeof(inst_t) * g->cap);
    }
    g->code[g->len].op = op;
    g->code[g->len].arg = arg;
    g->code[g->len].x = g->code[g->len].y = 0;
    return g->len++;
}

/* ---- parser: alternation > concatenation > repetition > atom ---- */

static void parse_alt(parser_t *ps);

static int is_op(parser_t *ps, char c) {
    /* In BRE, operators are written with a backslash */
    if (ps->extended) return *ps->p == c;
    if (c == '*' || c == '^' || c == '$' || c == '.' || c == '[') return *ps->p == c;
    return ps->p[0] == '\\' && ps->p[1] == c;
}

static void skip_op(parser_t *ps, char c) {
    ps->p += (ps->extended || c == '*' || c == '^' || c == '$' || c == '.' || c == '[') ? 1 : 2;
}

static void parse_class(parser_t *ps) {
    prog_t *g = ps->prog;
    if (g->nclasses == g->class_cap) {
        g->class_cap = g->class_cap ? g->class_cap * 2 : 8;
        g->classes = realloc(g->classes, sizeof(*g->classes) * g->class_cap);
    }
    unsigned char *set = g->classes[g->nclasses];
    memset(set, 0, 32);
    ps->p++; /* '[' */
    int negate = 0;
    if (*ps->p == '^') {
        negate = 1;
        ps->p++;
    }
    int first = 1;
    while (*ps->p && (*ps->p != ']' || first)) {
        first = 0;
        if (ps->p[0] == '[' && ps->p[1] == ':') {
            /* Named classes such as [:alpha:] */
            const char *end = strstr(ps->p + 2, ":]");
            if (!end) { ps->error = REG_EBRACK; return; }
            int n = end - (ps->p + 2);
            for (int c = 0; c < 256; c++) {
                int in = 0;
                if (!strncmp(ps->p + 2, "alpha", n) && n == 5) in = isalpha(c);
                else if (!strncmp(ps->p + 2, "digit", n) && n == 5) in = isdigit(c);
                else if (!strncmp(ps->p + 2, "alnum", n) && n == 5) in = isalnum(c);
                else if (!strncmp(ps->p + 2, "space", n) && n == 5) in = isspace(c);
                else if (!strncmp(ps->p + 2, "upper", n) && n == 5) in = isupper(c);
                else if (!strncmp(ps->p + 2, "lower", n) && n == 5) in = islower(c);
                else if (!strncmp(ps->p + 2, "punct", n) && n == 5) in = ispunct(c);
                else if (!strncmp(ps->p + 2, "xdigit", n) && n == 6) in = isxdigit(c);
                else if (!strncmp(ps->p + 2, "blank", n) && n == 5) in = (c == ' ' || c == '\t');
                if (in) set[c >> 3] |= 1 << (c & 7);
            }
            ps->p = end + 2;
            continue;
        }
        int lo = (unsigned char)*ps->p++;
        int hi = lo;
        if (ps->p[0] == '-' && ps->p[1] && ps->p[1] != ']') {
            hi = (unsigned char)ps->p[1];
            ps->p += 2;
        }
        for (int c = lo; c <= hi; c++) set[c >> 3] |= 1 << (c & 7);
    }
    if (*ps->p != ']') {
        ps->error = REG_EBRACK;
        return;
    }
    ps->p++;
    if (g->icase) {
        for (int c = 0; c < 256; c++) {
            if (set[c >> 3] & (1 << (c & 7))) {
                int o = isupper(c) ? tolower(c) : toupper(c);
                set[o >> 3] |= 1 << (o & 7);
            }
        }
    }
    if (negate) {
        for (int i = 0; i < 32; i++) set[i] = ~set[i];
        set[0] &= ~1; /* never match NUL */
    }
    emit(g, OP_CLASS, g->nclasses++);
}

/* Parse one atom; returns 0 at the end of a concatenation */
static int parse_atom(parser_t *ps) {
    prog_t *g = ps->prog;
    if (!*ps->p || is_op(ps, '|') || is_op(ps, ')')) return 0;

    if (is_op(ps, '(')) {
        skip_op(ps, '(');
        parse_alt(ps);
        if (!is_op(ps, ')')) {
            ps->error = REG_BADPAT;
            return 0;
        }
        skip_op(ps, ')');
    } else if (is_op(ps, '.')) {
        ps->p++;
        emit(g, OP_ANY, 0);
    } else if (is_op(ps, '[')) {
        parse_class(ps);
    } else if (is_op(ps, '^')) {
        ps->p++;
        emit(g, OP_BOL, 0);
    } else if (is_op(ps, '$')) {
        ps->p++;
        emit(g, OP_EOL, 0);
    } else {
        int c = (unsigned char)*ps->p++;
        if (c == '\\' && *ps->p) {
            c = (unsigned char)*ps->p++;
            if (c == 'n') c = '\n';
            else if (c == 't') c = '\t';
        }
        emit(g, OP_CHAR, g->icase ? tolower(c) : c);
    }
    return 1;
}

/* Repeat the atom at [start, g->len) between min and max (-1 = unbounded) times */
static void apply_repeat(prog_t *g, int start, int min, int max) {
    int end = g->len;
    int atom_len = end - start;
    /* Move the atom out of the way so the copies can be laid out in order */
    inst_t *atom = malloc(sizeof(inst_t) * (atom_len ? atom_len : 1));
    memcpy(atom, g->code + start, sizeof(inst_t) * atom_len);
    g->len = start;

    for (int i = 0; i < min; i++) {
        int s = g->len;
        for (int k = 0; k < atom_len; k++) {
            int n = emit(g, atom[k].op, atom[k].arg);
            g->code[n].x = atom[k].x - start + s;
            g->code[n].y = atom[k].y - start + s;
        }
    }
    if (max < 0) {
        /* L: split body, out; body; jmp L */
        int split = emit(g, OP_SPLIT, 0);
        int s = g->len;
        for (int k = 0; k < atom_len; k++) {
            int n = emit(g, atom[k].op, atom[k].arg);
            g->code[n].x = atom[k].x - start + s;
            g->code[n].y = atom[k].y - start + s;
        }
        int j = emit(g, OP_JMP, 0);
        g->code[j].x = split;
        g->code[split].x = s;
        g->code[split].y = g->len;
    } else {
        for (int i = min; i < max; i++) {
            int split = emit(g, OP_SPLIT, 0);
            int s = g->len;
            for (int k = 0; k < atom_len; k++) {
                int n = emit(g, atom[k].op, atom[k].arg);
                g->code[n].x = atom[k].x - start + s;
                g->code[n].y = atom[k].y - start + s;
            }
            g->code[split].x = s;
            g->code[split].y = -1; /* patched below to the very end */
        }
        for (int i = start; i < g->len; i++) {
            if (g->code[i].op == OP_SPLIT && g->code[i].y == -1) g->code[i].y = g->len;
        }
    }
    free(atom);
}

static void parse_concat(parser_t *ps) {
    prog_t *g = ps->prog;
    for (;;) {
        int start = g->len;
        if (!parse_atom(ps) || ps->error) return;
        for (;;) {
            int min, max;
            if (is_op(ps, '*')) { skip_op(ps, '*'); min = 0; max = -1; }
            else if (is_op(ps, '+')) { skip_op(ps, '+'); min = 1; max = -1; }
            else if (is_op(ps, '?')) { skip_op(ps, '?'); min = 0; max = 1; }
            else if (is_op(ps, '{')) {
                skip_op(ps, '{');
                min = strtol(ps->p, (char **)&ps->p, 10);
                max = min;
                if (*ps->p == ',') {
                    ps->p++;
                    max = (*ps->p >= '0' && *ps->p <= '9') ? strtol(ps->p, (char **)&ps->p, 10) : -1;
                }
                if (!is_op(ps, '}')) { ps->error = REG_BADPAT; return; }
                skip_op(ps, '}');
            } else break;
            apply_repeat(g, start, min, max);
        }
    }
}

static void parse_alt(parser_t *ps) {
    prog_t *g = ps->prog;
    int start = g->len;
    parse_concat(ps);
    while (!ps->error && is_op(ps, '|')) {
        skip_op(ps, '|');
        /* Rebuild in place as: split first, second; first; jmp end; second */
        int first_len = g->len - start;
        int split_at = start;
        inst_t *first = malloc(sizeof(inst_t) * (first_len ? first_len : 1));
        memcpy(first, g->code + start, sizeof(inst_t) * first_len);
        g->len = start;
        emit(g, OP_SPLIT, 0);
        int fs = g->len;
        for (int k = 0; k < first_len; k++) {
            int n = emit(g, first[k].op, first[k].arg);
            g->code[n].x = first[k].x - start + fs;
            g->code[n].y = first[k].y - start + fs;
        }
        free(first);
        int jmp = emit(g, OP_JMP, 0);
        int second = g->len;
        parse_concat(ps);
        g->code[split_at].x = fs;
        g->code[split_at].y = second;
        g->code[jmp].x = g->len;
    }
}

int regcomp(regex_t *preg, const char *regex, int cflags) {
    prog_t *g = calloc(1, sizeof(prog_t));
    if (!g) return REG_ESPACE;
    g->icase = (cflags & REG_ICASE) != 0;
    parser_t ps = { regex, (cflags & REG_EXTENDED) != 0, 0, g };
    parse_alt(&ps);
    if (!ps.error && *ps.p) ps.error = REG_BADPAT; /* unbalanced ')' */
    emit(g, OP_MATCH, 0);
    if (ps.error) {
        free(g->code);
        free(g->classes);
        free(g);
        return ps.error;
    }
    preg->re_prog = g;
    preg->re_nsub = 0;
    preg->re_cflags = cflags;
    return 0;
}

/* ---- Pike VM ---- */

typedef struct {
    int *pcs;
    int *starts;
    int n;
} threads_t;

static void add_thread(const prog_t *g, threads_t *t, int *seen, int gen, int pc, int start,
                       const char *s, const char *str, int eflags) {
    if (seen[pc] == gen) return;
    seen[pc] = gen;
    const inst_t *in = &g->code[pc];
    switch (in->op) {
    case OP_JMP:
        add_thread(g, t, seen, gen, in->x, start, s, str, eflags);
        return;
    case OP_SPLIT:
        add_thread(g, t, seen, gen, in->x, start, s, str, eflags);
        add_thread(g, t, seen, gen, in->y, start, s, str, eflags);
        return;
    case OP_BOL:
        if (s == str && !(eflags & REG_NOTBOL)) add_thread(g, t, seen, gen, pc + 1, start, s, str, eflags);
        return;
    case OP_EOL:
        if (*s == '\0' && !(eflags & REG_NOTEOL)) add_thread(g, t, seen, gen, pc + 1, start, s, str, eflags);
        return;
    }
    t->pcs[t->n] = pc;
    t->starts[t->n] = start;
    t->n++;
}

int regexec(const regex_t *preg, const char *string, size_t nmatch, regmatch_t pmatch[], int eflags) {
    const prog_t *g = preg->re_prog;
    int n = g->len;
    int *buf = malloc(sizeof(int) * n * 5);
    if (!buf) return REG_ESPACE;
    threads_t cur = { buf, buf + n, 0 }, next = { buf + 2 * n, buf + 3 * n, 0 };
    int *seen = buf + 4 * n;
    for (int i = 0; i < n; i++) seen[i] = -1;

    int best_start = -1, best_end = -1;
    int gen = 0;
    const char *s = string;
    for (;; s++) {
        /* Start a new attempt here unless a match has already begun earlier */
        if (best_start < 0) add_thread(g, &cur, seen, gen, 0, s - string, s, string, eflags);
        if (cur.n == 0) break;
        gen++;
        next.n = 0;
        int c = (unsigned char)*s;
        int lc = g->icase ? tolower(c) : c;
        for (int i = 0; i < cur.n; i++) {
            const inst_t *in = &g->code[cur.pcs[i]];
            int ok = 0;
            switch (in->op) {
            case OP_MATCH:
                /* Leftmost wins; among those, the longest */
                if (best_start < 0 || cur.starts[i] < best_start ||
                    (cur.starts[i] == best_start && (s - string) > best_end)) {
                    best_start = cur.starts[i];
                    best_end = s - string;
                }
                /* Lower-priority threads may still extend a match from the same start */
                continue;
            case OP_CHAR: ok = c && in->arg == lc; break;
            case OP_ANY: ok = c && !((preg->re_cflags & REG_NEWLINE) && c == '\n'); break;
            case OP_CLASS: ok = c && (g->classes[in->arg][c >> 3] & (1 << (c & 7))); break;
            }
            if (ok && (best_start < 0 || cur.starts[i] <= best_start)) {
                add_thread(g, &next, seen, gen, cur.pcs[i] + 1, cur.starts[i], s + 1, string, eflags);
            }
        }
        threads_t tmp = cur;
        cur = next;
        next = tmp;
        if (!*s) {
            /* Let threads that reached MATCH at the end record it */
            for (int i = 0; i < cur.n; i++) {
                if (g->code[cur.pcs[i]].op == OP_MATCH &&
                    (best_start < 0 || cur.starts[i] < best_start ||
                     (cur.starts[i] == best_start && (s - string) > best_end))) {
                    best_start = cur.starts[i];
                    best_end = s - string;
                }
            }
            break;
        }
    }
    free(buf);
    if (best_start < 0) return REG_NOMATCH;
    if (!(preg->re_cflags & REG_NOSUB) && nmatch > 0 && pmatch) {
        pmatch[0].rm_so = best_start;
        pmatch[0].rm_eo = best_end;
        for (size_t i = 1; i < nmatch; i++) pmatch[i].rm_so = pmatch[i].rm_eo = -1;
    }
    return 0;
}

size_t regerror(int errcode, const regex_t *preg, char *errbuf, size_t errbuf_size) {
    (void)preg;
    const char *msg = errcode == REG_NOMATCH ? "No match"
                    : errcode == REG_EBRACK ? "Unmatched [ or [^"
                    : errcode == REG_ESPACE ? "Out of memory"
                    : "Invalid regular expression";
    size_t len = strlen(msg) + 1;
    if (errbuf && errbuf_size) {
        strncpy(errbuf, msg, errbuf_size - 1);
        errbuf[errbuf_size - 1] = '\0';
    }
    return len;
}

void regfree(regex_t *preg) {
    prog_t *g = preg->re_prog;
    if (!g) return;
    free(g->code);
    free(g->classes);
    free(g);
    preg->re_prog = NULL;
}
