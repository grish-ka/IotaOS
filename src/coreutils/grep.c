/*
* IotaOS - grep.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "common.h"
#include <fcntl.h>
#include <ctype.h>

/*
 * Basic regular expressions: literals, '.', '*', '^', '$', [set], [^set],
 * and '\' escapes. -F treats the pattern as a fixed string.
 */

static int opt_icase, opt_invert, opt_number, opt_count, opt_list, opt_fixed, opt_quiet, opt_hide_name, opt_only;
static int use_color;

static int chr_eq(int a, int b) {
    return opt_icase ? tolower(a) == tolower(b) : a == b;
}

/* Length of the pattern atom at p */
static int atom_len(const char *p) {
    if (*p == '\\' && p[1]) return 2;
    if (*p == '[') {
        const char *q = p + 1;
        if (*q == '^') q++;
        if (*q == ']') q++;
        while (*q && *q != ']') q++;
        return *q ? (int)(q - p + 1) : 1;
    }
    return 1;
}

static int atom_matches(const char *p, int c) {
    if (c == '\0') return 0;
    if (*p == '\\') return chr_eq(p[1], c);
    if (*p == '.') return 1;
    if (*p == '[' && atom_len(p) > 1) {
        const char *q = p + 1;
        int negate = (*q == '^');
        if (negate) q++;
        int hit = 0;
        const char *first = q;
        while (*q && (*q != ']' || q == first)) {
            if (q[1] == '-' && q[2] && q[2] != ']') {
                int lo = q[0], hi = q[2];
                if ((c >= lo && c <= hi) || (opt_icase && tolower(c) >= tolower(lo) && tolower(c) <= tolower(hi))) hit = 1;
                q += 3;
            } else {
                if (chr_eq(*q, c)) hit = 1;
                q++;
            }
        }
        return hit != negate;
    }
    return chr_eq(*p, c);
}

/* Match pattern at text; returns the end of the match or NULL */
static const char *match_here(const char *re, const char *text) {
    if (*re == '\0') return text;
    if (re[0] == '$' && re[1] == '\0') return *text == '\0' ? text : NULL;

    int len = atom_len(re);
    if (re[len] == '*') {
        const char *t = text;
        while (atom_matches(re, *t)) t++;
        for (; t >= text; t--) {
            const char *end = match_here(re + len + 1, t);
            if (end) return end;
        }
        return NULL;
    }
    if (atom_matches(re, *text)) return match_here(re + len, text + 1);
    return NULL;
}

/* Find the first match in text; sets *start and returns its end, or NULL */
static const char *find_match(const char *re, const char *text, const char **start) {
    if (opt_fixed) {
        size_t len = strlen(re);
        for (const char *t = text; *t; t++) {
            size_t i = 0;
            while (i < len && t[i] && chr_eq(re[i], t[i])) i++;
            if (i == len) {
                *start = t;
                return t + len;
            }
        }
        if (len == 0) {
            *start = text;
            return text;
        }
        return NULL;
    }
    if (re[0] == '^') {
        *start = text;
        return match_here(re + 1, text);
    }
    const char *t = text;
    do {
        const char *end = match_here(re, t);
        if (end) {
            *start = t;
            return end;
        }
    } while (*t++);
    return NULL;
}

static void print_line(const char *re, const char *line) {
    if (!use_color || opt_invert) {
        puts(line);
        return;
    }
    const char *p = line, *start, *end;
    while (*p && (end = find_match(re, p, &start)) != NULL) {
        if (end == start) break;
        fwrite(p, 1, start - p, stdout);
        printf("\033[1;31m");
        fwrite(start, 1, end - start, stdout);
        printf("\033[0m");
        p = end;
    }
    puts(p);
}

static int grep_fd(const char *re, int fd, const char *name, int show_name) {
    FILE *f = fdopen(fd, "r");
    char *line = NULL;
    size_t cap = 0;
    ssize_t n;
    long lineno = 0, matches = 0;
    while ((n = getline(&line, &cap, f)) > 0) {
        lineno++;
        if (line[n - 1] == '\n') line[n - 1] = '\0';
        const char *start;
        int hit = find_match(re, line, &start) != NULL;
        if (hit == opt_invert) continue;
        matches++;
        if (opt_quiet) break;
        if (opt_list) {
            printf("%s\n", name);
            break;
        }
        if (opt_count) continue;
        if (show_name) printf(use_color ? "\033[35m%s\033[36m:\033[0m" : "%s:", name);
        if (opt_number) printf(use_color ? "\033[32m%ld\033[36m:\033[0m" : "%ld:", lineno);
        if (opt_only && !opt_invert) {
            const char *s, *e;
            const char *p = line;
            while ((e = find_match(re, p, &s)) != NULL && e != s) {
                printf(use_color ? "\033[1;31m%.*s\033[0m\n" : "%.*s\n", (int)(e - s), s);
                p = e;
            }
            continue;
        }
        print_line(re, line);
    }
    if (opt_count) {
        if (show_name) printf("%s:", name);
        printf("%ld\n", matches);
    }
    free(line);
    return matches > 0;
}

int main(int argc, char *argv[]) {
    const char *prog = prog_name(argv[0]);
    use_color = isatty(STDOUT_FILENO);
    if (!strcmp(prog, "fgrep")) opt_fixed = 1;

    int i = 1;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        if (!strcmp(argv[i], "--")) { i++; break; }
        if (!strncmp(argv[i], "--color", 7)) {
            const char *when = strchr(argv[i], '=');
            use_color = !when || !strcmp(when + 1, "always") || (!strcmp(when + 1, "auto") && isatty(STDOUT_FILENO));
            continue;
        }
        for (char *p = argv[i] + 1; *p; p++) {
            switch (*p) {
            case 'i': opt_icase = 1; break;
            case 'v': opt_invert = 1; break;
            case 'n': opt_number = 1; break;
            case 'c': opt_count = 1; break;
            case 'l': opt_list = 1; break;
            case 'F': opt_fixed = 1; break;
            case 'q': opt_quiet = 1; break;
            case 'h': opt_hide_name = 1; break;
            case 'o': opt_only = 1; break;
            case 'E': case 'G': break;
            default:
                fprintf(stderr, "%s: invalid option -- '%c'\n", prog, *p);
                return 2;
            }
        }
    }
    if (i >= argc) {
        fprintf(stderr, "Usage: %s [-cFhilnoqv] PATTERN [FILE]...\n", prog);
        return 2;
    }
    const char *re = argv[i++];

    int found = 0, status_err = 0;
    if (i >= argc) {
        found = grep_fd(re, STDIN_FILENO, "(standard input)", 0);
    } else {
        int show_name = (argc - i > 1) && !opt_hide_name;
        for (; i < argc; i++) {
            int fd = open(argv[i], O_RDONLY);
            if (fd < 0) {
                fprintf(stderr, "%s: %s: %s\n", prog, argv[i], strerror(errno));
                status_err = 1;
                continue;
            }
            found |= grep_fd(re, fd, argv[i], show_name);
            close(fd);
        }
    }
    if (status_err && !opt_quiet) return 2;
    return found ? 0 : 1;
}
