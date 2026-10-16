/*
* IotaOS - ls.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "common.h"
#include <dirent.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>

static int opt_all, opt_almost_all, opt_long, opt_one, opt_human, opt_dir, opt_recursive, opt_classify;
static int opt_reverse, opt_sort_time, opt_sort_size, opt_inode;
static int use_color;
static int status;
static const char *prog;

typedef struct {
    char *name;
    struct stat st;
} entry_t;

static void mode_string(mode_t m, char *out) {
    out[0] = S_ISDIR(m) ? 'd' : S_ISCHR(m) ? 'c' : S_ISFIFO(m) ? 'p' : '-';
    const char *rwx = "rwxrwxrwx";
    for (int i = 0; i < 9; i++) out[i + 1] = (m & (0400 >> i)) ? rwx[i] : '-';
    if (m & S_ISUID) out[3] = (m & 0100) ? 's' : 'S';
    if (m & S_ISGID) out[6] = (m & 0010) ? 's' : 'S';
    if (m & S_ISVTX) out[9] = (m & 0001) ? 't' : 'T';
    out[10] = '\0';
}

/* GNU ls colours: directories bold blue, executables bold green, devices yellow */
static const char *color_of(mode_t m) {
    if (S_ISDIR(m)) return (m & S_ISVTX) ? "\033[30;42m" : "\033[1;34m";
    if (S_ISCHR(m)) return "\033[1;33m";
    if (S_ISFIFO(m)) return "\033[33m";
    if (m & S_ISUID) return "\033[37;41m";
    if (m & S_ISGID) return "\033[30;43m";
    if (m & 0111) return "\033[1;32m";
    return NULL;
}

static char classify_char(mode_t m) {
    if (S_ISDIR(m)) return '/';
    if (S_ISFIFO(m)) return '|';
    if (m & 0111) return '*';
    return 0;
}

/* Visible width of a name as printed (with its -F marker) */
static int name_width(const entry_t *e) {
    return strlen(e->name) + (opt_classify && classify_char(e->st.st_mode) ? 1 : 0);
}

static void print_name(const entry_t *e) {
    const char *c = use_color ? color_of(e->st.st_mode) : NULL;
    if (c) printf("%s%s\033[0m", c, e->name);
    else printf("%s", e->name);
    if (opt_classify) {
        char k = classify_char(e->st.st_mode);
        if (k) putchar(k);
    }
}

static int compare(const void *a, const void *b) {
    const entry_t *x = a, *y = b;
    int r;
    if (opt_sort_time && x->st.st_mtime != y->st.st_mtime) r = (y->st.st_mtime > x->st.st_mtime) ? 1 : -1;
    else if (opt_sort_size && x->st.st_size != y->st.st_size) r = (y->st.st_size > x->st.st_size) ? 1 : -1;
    else r = strcmp(x->name, y->name);
    return opt_reverse ? -r : r;
}

static void print_long(entry_t *list, int n) {
    int wlink = 1, wsize = 1, wino = 1;
    char buf[32];
    for (int i = 0; i < n; i++) {
        int l = snprintf(buf, sizeof(buf), "%d", list[i].st.st_nlink);
        if (l > wlink) wlink = l;
        if (opt_human) human_size(list[i].st.st_size, buf, sizeof(buf));
        else snprintf(buf, sizeof(buf), "%ld", list[i].st.st_size);
        l = strlen(buf);
        if (l > wsize) wsize = l;
        l = snprintf(buf, sizeof(buf), "%d", list[i].st.st_ino);
        if (l > wino) wino = l;
    }

    time_t now = time(NULL);
    for (int i = 0; i < n; i++) {
        entry_t *e = &list[i];
        char mode[12], size[32], date[32];
        mode_string(e->st.st_mode, mode);
        if (opt_human) human_size(e->st.st_size, size, sizeof(size));
        else snprintf(size, sizeof(size), "%ld", e->st.st_size);

        /* Like GNU: show the year instead of the time for old or future files */
        time_t t = e->st.st_mtime;
        struct tm tm;
        localtime_r(&t, &tm);
        if (t > now + 3600 || now - t > 180L * 24 * 3600) strftime(date, sizeof(date), "%b %e  %Y", &tm);
        else strftime(date, sizeof(date), "%b %e %H:%M", &tm);

        struct passwd *pw = getpwuid(e->st.st_uid);
        char user[32];
        snprintf(user, sizeof(user), "%s", pw ? pw->pw_name : "?");
        struct group *gr = getgrgid(e->st.st_gid);
        char group[32];
        snprintf(group, sizeof(group), "%s", gr ? gr->gr_name : "root");

        if (opt_inode) printf("%*d ", wino, e->st.st_ino);
        printf("%s %*d %s %s %*s %s ", mode, wlink, e->st.st_nlink, user, group, wsize, size, date);
        print_name(e);
        putchar('\n');
    }
}

static int terminal_width(void) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) return ws.ws_col;
    return 80;
}

/* GNU-style columns, filled top to bottom */
static void print_columns(entry_t *list, int n) {
    if (n == 0) return;
    if (opt_one || !isatty(STDOUT_FILENO)) {
        for (int i = 0; i < n; i++) {
            if (opt_inode) printf("%d ", list[i].st.st_ino);
            print_name(&list[i]);
            putchar('\n');
        }
        return;
    }

    int width = terminal_width();
    int cols, rows;
    int colw[64];
    for (cols = n < 64 ? n : 64; cols > 1; cols--) {
        rows = (n + cols - 1) / cols;
        int total = 0;
        for (int c = 0; c < cols; c++) {
            colw[c] = 0;
            for (int r = 0; r < rows; r++) {
                int i = c * rows + r;
                if (i < n && name_width(&list[i]) > colw[c]) colw[c] = name_width(&list[i]);
            }
            total += colw[c] + (c < cols - 1 ? 2 : 0);
        }
        if (total < width) break;
    }
    if (cols <= 1) {
        cols = 1;
        rows = n;
    } else {
        rows = (n + cols - 1) / cols;
    }

    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            int i = c * rows + r;
            if (i >= n) break;
            print_name(&list[i]);
            int next = (c + 1) * rows + r;
            if (c < cols - 1 && next < n) {
                for (int pad = name_width(&list[i]); pad < colw[c] + 2; pad++) putchar(' ');
            }
        }
        putchar('\n');
    }
}

static void print_list(entry_t *list, int n) {
    qsort(list, n, sizeof(entry_t), compare);
    if (opt_long) print_long(list, n);
    else print_columns(list, n);
}

static int list_dir(const char *path, int show_header, int first) {
    DIR *d = opendir(path);
    if (!d) {
        fprintf(stderr, "%s: cannot open directory '%s': %s\n", prog, path, strerror(errno));
        status = 2;
        return -1;
    }

    int cap = 64, n = 0;
    entry_t *list = malloc(sizeof(entry_t) * cap);
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') {
            if (!opt_all && !opt_almost_all) continue;
            if (opt_almost_all && !opt_all && (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))) continue;
        }
        if (n == cap) {
            cap *= 2;
            list = realloc(list, sizeof(entry_t) * cap);
        }
        char full[512];
        path_join(full, sizeof(full), path, de->d_name);
        list[n].name = strdup(de->d_name);
        if (lstat(full, &list[n].st) < 0) memset(&list[n].st, 0, sizeof(struct stat));
        n++;
    }
    closedir(d);

    if (show_header) {
        if (!first) putchar('\n');
        printf("%s:\n", path);
    }
    if (opt_long) {
        long blocks = 0;
        for (int i = 0; i < n; i++) blocks += list[i].st.st_blocks;
        printf("total %ld\n", blocks / 2);
    }
    print_list(list, n);

    if (opt_recursive) {
        for (int i = 0; i < n; i++) {
            if (!S_ISDIR(list[i].st.st_mode) || !strcmp(list[i].name, ".") || !strcmp(list[i].name, "..")) continue;
            char full[512];
            path_join(full, sizeof(full), path, list[i].name);
            list_dir(full, 1, 0);
        }
    }

    for (int i = 0; i < n; i++) free(list[i].name);
    free(list);
    return 0;
}

int main(int argc, char *argv[]) {
    prog = prog_name(argv[0]);
    use_color = isatty(STDOUT_FILENO);

    int i = 1;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        if (!strcmp(argv[i], "--")) { i++; break; }
        if (!strncmp(argv[i], "--color", 7)) {
            const char *when = strchr(argv[i], '=');
            use_color = !when || !strcmp(when + 1, "always") || (!strcmp(when + 1, "auto") && isatty(STDOUT_FILENO));
            continue;
        }
        if (!strcmp(argv[i], "--all")) { opt_all = 1; continue; }
        if (!strcmp(argv[i], "--human-readable")) { opt_human = 1; continue; }
        for (char *p = argv[i] + 1; *p; p++) {
            switch (*p) {
            case 'a': opt_all = 1; break;
            case 'A': opt_almost_all = 1; break;
            case 'l': opt_long = 1; break;
            case '1': opt_one = 1; break;
            case 'h': opt_human = 1; break;
            case 'd': opt_dir = 1; break;
            case 'R': opt_recursive = 1; break;
            case 'F': opt_classify = 1; break;
            case 'r': opt_reverse = 1; break;
            case 't': opt_sort_time = 1; break;
            case 'S': opt_sort_size = 1; break;
            case 'i': opt_inode = 1; break;
            default:
                fprintf(stderr, "%s: invalid option -- '%c'\n", prog, *p);
                fprintf(stderr, "Usage: %s [-aAdFhilrRSt1] [FILE]...\n", prog);
                return 2;
            }
        }
    }

    char *dot[] = {"."};
    char **paths = (i < argc) ? argv + i : dot;
    int npaths = (i < argc) ? argc - i : 1;

    /* Files named on the command line are listed first, then directories */
    entry_t *files = malloc(sizeof(entry_t) * npaths);
    int nfiles = 0;
    for (int k = 0; k < npaths; k++) {
        struct stat st;
        if (lstat(paths[k], &st) < 0) {
            fprintf(stderr, "%s: cannot access '%s': %s\n", prog, paths[k], strerror(errno));
            status = 2;
            continue;
        }
        if (!S_ISDIR(st.st_mode) || opt_dir) {
            files[nfiles].name = paths[k];
            files[nfiles].st = st;
            nfiles++;
        }
    }
    print_list(files, nfiles);

    int first = (nfiles == 0);
    for (int k = 0; k < npaths; k++) {
        struct stat st;
        if (opt_dir || stat(paths[k], &st) < 0 || !S_ISDIR(st.st_mode)) continue;
        list_dir(paths[k], npaths > 1 || opt_recursive, first);
        first = 0;
    }
    return status;
}
