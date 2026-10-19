/*
* IotaOS - getopt.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*
* getopt(), getopt_long() and getopt_long_only() with GNU behaviour:
* options may follow operands ("ls dir -l") unless the option string starts
* with '+' or POSIXLY_CORRECT is set; '-' returns operands as option 1;
* "--" ends option parsing; long options may be abbreviated.
*/

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Weak, like the functions below: programs that bundle their own getopt
   (gawk, for one) override all of it */
__attribute__((weak)) char *optarg = NULL;
__attribute__((weak)) int optind = 1;
__attribute__((weak)) int opterr = 1;
__attribute__((weak)) int optopt = '?';
__attribute__((weak)) int optreset = 0;

static int nextchar;        /* position inside a group of short options */
static int first_nonopt;    /* operands moved aside: [first_nonopt, last_nonopt) */
static int last_nonopt;

/* Rotate argv so the operands skipped so far come after the options seen */
static void permute(char **argv) {
    int bottom = first_nonopt, middle = last_nonopt, top = optind;
    while (top > middle && middle > bottom) {
        if (top - middle > middle - bottom) {
            int len = middle - bottom;
            for (int i = 0; i < len; i++) {
                char *t = argv[bottom + i];
                argv[bottom + i] = argv[top - len + i];
                argv[top - len + i] = t;
            }
            top -= len;
        } else {
            int len = top - middle;
            for (int i = 0; i < len; i++) {
                char *t = argv[bottom + i];
                argv[bottom + i] = argv[middle + i];
                argv[middle + i] = t;
            }
            bottom += len;
        }
    }
    first_nonopt += optind - last_nonopt;
    last_nonopt = optind;
}

static int is_option(const char *arg) {
    return arg[0] == '-' && arg[1] != '\0';
}

static int parse_long(int argc, char *const argv[], const char *optstring,
                      const struct option *longopts, int *longindex, int prefix_len) {
    char *name = argv[optind] + prefix_len;
    char *eq = strchr(name, '=');
    size_t len = eq ? (size_t)(eq - name) : strlen(name);
    int found = -1, ambiguous = 0;

    for (int i = 0; longopts[i].name; i++) {
        if (strncmp(longopts[i].name, name, len)) continue;
        if (strlen(longopts[i].name) == len) { /* exact match wins */
            found = i;
            ambiguous = 0;
            break;
        }
        if (found < 0) {
            found = i;
        } else if (longopts[i].has_arg != longopts[found].has_arg ||
                   longopts[i].flag != longopts[found].flag ||
                   longopts[i].val != longopts[found].val) {
            ambiguous = 1;
        }
    }

    int colon = optstring[0] == ':';
    optind++;
    if (ambiguous) {
        if (opterr && !colon) fprintf(stderr, "%s: option '%s' is ambiguous\n", argv[0], argv[optind - 1]);
        optopt = 0;
        return '?';
    }
    if (found < 0) return -2; /* not a long option */

    const struct option *o = &longopts[found];
    if (longindex) *longindex = found;
    if (eq) {
        if (o->has_arg == no_argument) {
            if (opterr && !colon) fprintf(stderr, "%s: option '--%s' doesn't allow an argument\n", argv[0], o->name);
            optopt = o->val;
            return '?';
        }
        optarg = eq + 1;
    } else if (o->has_arg == required_argument) {
        if (optind < argc) {
            optarg = argv[optind++];
        } else {
            if (opterr && !colon) fprintf(stderr, "%s: option '--%s' requires an argument\n", argv[0], o->name);
            optopt = o->val;
            return colon ? ':' : '?';
        }
    } else {
        optarg = NULL;
    }
    if (o->flag) {
        *o->flag = o->val;
        return 0;
    }
    return o->val;
}

static int getopt_internal(int argc, char *const argv[], const char *optstring,
                           const struct option *longopts, int *longindex, int long_only) {
    char **args = (char **)argv;
    if (optind == 0 || optreset) {
        optind = 1;
        optreset = 0;
        nextchar = 0;
        first_nonopt = last_nonopt = 1;
    }
    if (optind == 1 && nextchar == 0) first_nonopt = last_nonopt = 1;

    int ordering = 0; /* 0 permute, 1 stop at first operand, 2 return operands */
    if (*optstring == '+') { ordering = 1; optstring++; }
    else if (*optstring == '-') { ordering = 2; optstring++; }
    else if (getenv("POSIXLY_CORRECT")) ordering = 1;
    int colon = optstring[0] == ':';

    optarg = NULL;
    if (nextchar == 0) {
        if (last_nonopt > optind) last_nonopt = optind;
        if (first_nonopt > optind) first_nonopt = optind;

        if (ordering == 0) {
            if (first_nonopt != last_nonopt && last_nonopt != optind) permute(args);
            else if (last_nonopt != optind) first_nonopt = optind;
            while (optind < argc && !is_option(argv[optind])) optind++;
            last_nonopt = optind;
        }

        if (optind != argc && !strcmp(argv[optind], "--")) {
            optind++;
            if (first_nonopt != last_nonopt && last_nonopt != optind) permute(args);
            else if (first_nonopt == last_nonopt) first_nonopt = optind;
            last_nonopt = argc;
            optind = argc;
        }

        if (optind >= argc) {
            if (first_nonopt != last_nonopt) optind = first_nonopt;
            return -1;
        }
        if (!is_option(argv[optind])) {
            if (ordering == 1) return -1;
            optarg = argv[optind++];
            return 1;
        }

        if (longopts) {
            if (argv[optind][1] == '-') {
                int r = parse_long(argc, argv, optstring, longopts, longindex, 2);
                if (r != -2) return r;
                if (opterr && !colon) fprintf(stderr, "%s: unrecognized option '%s'\n", argv[0], argv[optind - 1]);
                optopt = 0;
                return '?';
            }
            if (long_only && (argv[optind][2] || !strchr(optstring, argv[optind][1]))) {
                /* "-name" is tried as a long option unless it is a known short one alone */
                int save = optind;
                int r = parse_long(argc, argv, optstring, longopts, longindex, 1);
                if (r != -2) return r;
                /* Long lookup failed. Like glibc: retry as a short-option
                 * group only when the first char is a known short option
                 * ("-ax" -> 'a', then the usual '?x'); otherwise the whole
                 * argument is ONE unrecognized option: '?' with optopt = 0,
                 * consumed (-2 leaves optind already advanced past it by
                 * parse_long), never a chain of bogus short options. */
                if (strchr(optstring, argv[save][1])) {
                    optind = save;
                } else {
                    if (opterr && !colon) fprintf(stderr, "%s: unrecognized option '%s'\n", argv[0], argv[save]);
                    optopt = 0;
                    return '?';
                }
            }
        }
        nextchar = 1;
    }

    char *arg = argv[optind];
    int c = (unsigned char)arg[nextchar++];
    const char *spec = (c == ':') ? NULL : strchr(optstring, c);
    if (arg[nextchar] == '\0') {
        optind++;
        nextchar = 0;
    }

    if (!spec) {
        optopt = c;
        if (opterr && !colon) fprintf(stderr, "%s: invalid option -- '%c'\n", argv[0], c);
        return '?';
    }
    if (spec[1] == ':') {
        if (spec[2] == ':') {                 /* optional argument: only attached */
            if (nextchar) {
                optarg = arg + nextchar;
                optind++;
                nextchar = 0;
            } else {
                optarg = NULL;
            }
        } else if (nextchar) {
            optarg = arg + nextchar;
            optind++;
            nextchar = 0;
        } else if (optind < argc) {
            optarg = argv[optind++];
        } else {
            optopt = c;
            if (opterr && !colon) fprintf(stderr, "%s: option requires an argument -- '%c'\n", argv[0], c);
            return colon ? ':' : '?';
        }
    }
    return c;
}

__attribute__((weak)) int getopt(int argc, char *const argv[], const char *optstring) {
    return getopt_internal(argc, argv, optstring, NULL, NULL, 0);
}

__attribute__((weak)) int getopt_long(int argc, char *const argv[], const char *optstring,
                const struct option *longopts, int *longindex) {
    return getopt_internal(argc, argv, optstring, longopts, longindex, 0);
}

__attribute__((weak)) int getopt_long_only(int argc, char *const argv[], const char *optstring,
                     const struct option *longopts, int *longindex) {
    return getopt_internal(argc, argv, optstring, longopts, longindex, 1);
}
