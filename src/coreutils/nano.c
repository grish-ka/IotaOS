/*
* IotaOS - nano.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include "common.h"
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>

#define ROWS 25
#define COLS 80
#define EDIT_ROWS (ROWS - 4)   /* title bar, status line and two help lines */
#define TAB_WIDTH 8

typedef struct {
    char *text;
    int len;
    int cap;
} line_t;

static line_t *lines;
static int nlines, lines_cap;
static int cx, cy;            /* cursor: byte column and line */
static int top, left;         /* first line / first screen column shown */
static int modified;
static char filename[256];
static char status[128];
static char *cut_buffer;
static struct termios saved_tty;

/* ---- output buffering: the whole screen goes out in one write ---- */

static char out[ROWS * COLS * 8];
static int out_len;

static void emit(const char *s, int n) {
    if (out_len + n > (int)sizeof(out)) {
        write(STDOUT_FILENO, out, out_len);
        out_len = 0;
    }
    memcpy(out + out_len, s, n);
    out_len += n;
}

static void emits(const char *s) {
    emit(s, strlen(s));
}

static void flush_out(void) {
    write(STDOUT_FILENO, out, out_len);
    out_len = 0;
}

/* ---- text buffer ---- */

static line_t *insert_line(int at) {
    if (nlines == lines_cap) {
        lines_cap = lines_cap ? lines_cap * 2 : 64;
        lines = realloc(lines, sizeof(line_t) * lines_cap);
    }
    memmove(&lines[at + 1], &lines[at], sizeof(line_t) * (nlines - at));
    nlines++;
    lines[at].cap = 16;
    lines[at].len = 0;
    lines[at].text = malloc(lines[at].cap);
    return &lines[at];
}

static void delete_line(int at) {
    free(lines[at].text);
    memmove(&lines[at], &lines[at + 1], sizeof(line_t) * (nlines - at - 1));
    nlines--;
}

static void line_reserve(line_t *l, int need) {
    if (need <= l->cap) return;
    while (l->cap < need) l->cap *= 2;
    l->text = realloc(l->text, l->cap);
}

static void line_insert(line_t *l, int at, const char *s, int n) {
    line_reserve(l, l->len + n + 1);
    memmove(l->text + at + n, l->text + at, l->len - at);
    memcpy(l->text + at, s, n);
    l->len += n;
}

static void load_file(const char *path) {
    nlines = 0;
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        insert_line(0);
        snprintf(status, sizeof(status), "[ New File ]");
        return;
    }
    line_t *cur = insert_line(0);
    char buf[4096];
    ssize_t n;
    long count = 1;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        for (ssize_t i = 0; i < n; i++) {
            if (buf[i] == '\n') {
                cur = insert_line(nlines);
                count++;
            } else if (buf[i] != '\r') {
                line_insert(cur, cur->len, &buf[i], 1);
            }
        }
    }
    close(fd);
    /* A trailing newline does not start another line */
    if (nlines > 1 && lines[nlines - 1].len == 0) {
        delete_line(nlines - 1);
        count--;
    }
    snprintf(status, sizeof(status), "[ Read %ld line%s ]", count, count == 1 ? "" : "s");
}

static int save_file(const char *path) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        snprintf(status, sizeof(status), "[ Error writing %s: %s ]", path, strerror(errno));
        return -1;
    }
    for (int i = 0; i < nlines; i++) {
        write(fd, lines[i].text, lines[i].len);
        write(fd, "\n", 1);
    }
    close(fd);
    modified = 0;
    snprintf(status, sizeof(status), "[ Wrote %d line%s ]", nlines, nlines == 1 ? "" : "s");
    return 0;
}

/* ---- screen ---- */

/* Screen column of byte offset col in a line (tabs expand) */
static int render_col(const line_t *l, int col) {
    int x = 0;
    for (int i = 0; i < col && i < l->len; i++) {
        x += (l->text[i] == '\t') ? TAB_WIDTH - (x % TAB_WIDTH) : 1;
    }
    return x;
}

static void goto_xy(int row, int col) {
    char buf[24];
    snprintf(buf, sizeof(buf), "\033[%d;%dH", row + 1, col + 1);
    emits(buf);
}

static void draw_bar(int row, const char *text) {
    goto_xy(row, 0);
    emits("\033[7m");
    char buf[COLS + 1];
    snprintf(buf, sizeof(buf), "%-*.*s", COLS - 1, COLS - 1, text);
    emits(buf);
    emits(" \033[0m");
}

static void draw_shortcuts(int row, const char *const items[][2], int count) {
    goto_xy(row, 0);
    emits("\033[K");
    for (int i = 0; i < count; i++) {
        emits("\033[7m");
        emits(items[i][0]);
        emits("\033[0m ");
        char buf[24];
        snprintf(buf, sizeof(buf), "%-10s", items[i][1]);
        emits(buf);
    }
}

static void scroll_into_view(void) {
    if (cy < top) top = cy;
    if (cy >= top + EDIT_ROWS) top = cy - EDIT_ROWS + 1;
    int rx = render_col(&lines[cy], cx);
    if (rx < left) left = rx;
    if (rx >= left + COLS - 1) left = rx - COLS + 2;
}

static void draw(void) {
    scroll_into_view();
    emits("\033[H");

    char title[COLS + 1];
    snprintf(title, sizeof(title), "  GNU nano 7.2%*s%-30s%s", 12, "",
             filename[0] ? filename : "New Buffer", modified ? "Modified" : "");
    draw_bar(0, title);

    for (int r = 0; r < EDIT_ROWS; r++) {
        goto_xy(r + 1, 0);
        int li = top + r;
        if (li < nlines) {
            const line_t *l = &lines[li];
            char row[COLS + 1];
            int n = 0, x = 0;
            /* At most COLS-1 columns, so the cursor never sits in the wrap column */
            for (int i = 0; i < l->len && n < COLS - 1; i++) {
                int w = (l->text[i] == '\t') ? TAB_WIDTH - (x % TAB_WIDTH) : 1;
                for (int k = 0; k < w && n < COLS - 1; k++, x++) {
                    if (x >= left) row[n++] = (l->text[i] == '\t') ? ' ' : l->text[i];
                }
            }
            emit(row, n);
            if (left > 0 && n > 0) {
                goto_xy(r + 1, 0);
                emits("$");
            }
        }
        emits("\033[K");
    }

    goto_xy(ROWS - 3, 0);
    emits("\033[K");
    if (status[0]) {
        int pad = (COLS - (int)strlen(status)) / 2;
        goto_xy(ROWS - 3, pad > 0 ? pad : 0);
        emits("\033[7m");
        emits(status);
        emits("\033[0m");
    }

    static const char *const row1[][2] = {
        {"^G", "Help"}, {"^O", "Write Out"}, {"^W", "Where Is"}, {"^K", "Cut"}, {"^C", "Location"},
    };
    static const char *const row2[][2] = {
        {"^X", "Exit"}, {"^R", "Read File"}, {"^\\", "Replace"}, {"^U", "Paste"}, {"^_", "Go To Line"},
    };
    draw_shortcuts(ROWS - 2, row1, 5);
    draw_shortcuts(ROWS - 1, row2, 5);

    goto_xy(cy - top + 1, render_col(&lines[cy], cx) - left);
    flush_out();
}

/* ---- input ---- */

enum { KEY_UP = 1000, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_HOME, KEY_END, KEY_DEL, KEY_PGUP, KEY_PGDN };

static int read_key(void) {
    unsigned char c;
    if (read(STDIN_FILENO, &c, 1) != 1) return -1;
    if (c != 0x1b) return c;

    unsigned char seq[3];
    if (read(STDIN_FILENO, &seq[0], 1) != 1) return 0x1b;
    if (seq[0] != '[') return 0x1b;
    if (read(STDIN_FILENO, &seq[1], 1) != 1) return 0x1b;
    if (seq[1] >= '0' && seq[1] <= '9') {
        if (read(STDIN_FILENO, &seq[2], 1) != 1) return 0x1b;
        switch (seq[1]) {
        case '1': case '7': return KEY_HOME;
        case '4': case '8': return KEY_END;
        case '3': return KEY_DEL;
        case '5': return KEY_PGUP;
        case '6': return KEY_PGDN;
        }
        return 0;
    }
    switch (seq[1]) {
    case 'A': return KEY_UP;
    case 'B': return KEY_DOWN;
    case 'C': return KEY_RIGHT;
    case 'D': return KEY_LEFT;
    case 'H': return KEY_HOME;
    case 'F': return KEY_END;
    }
    return 0;
}

/* Ask a question on the status line; returns the answer or NULL if cancelled */
static char *prompt(const char *question, const char *initial) {
    static char answer[256];
    snprintf(answer, sizeof(answer), "%s", initial ? initial : "");
    int len = strlen(answer);
    for (;;) {
        goto_xy(ROWS - 3, 0);
        emits("\033[7m");
        char buf[COLS + 1];
        snprintf(buf, sizeof(buf), "%s: %-*s", question, (int)(COLS - 3 - strlen(question)), answer);
        emits(buf);
        emits("\033[0m");
        goto_xy(ROWS - 3, strlen(question) + 2 + len);
        flush_out();

        int k = read_key();
        if (k == '\r' || k == '\n') return answer;
        if (k == 0x03 || k == 0x07 || k == -1) return NULL; /* ^C, ^G */
        if ((k == 0x08 || k == 0x7f) && len > 0) answer[--len] = '\0';
        else if (k >= 0x20 && k < 0x7f && len < (int)sizeof(answer) - 1) {
            answer[len++] = (char)k;
            answer[len] = '\0';
        }
    }
}

static int ask_yes_no(const char *question) {
    for (;;) {
        goto_xy(ROWS - 3, 0);
        emits("\033[7m");
        char buf[COLS + 1];
        snprintf(buf, sizeof(buf), "%-*s", COLS - 1, question);
        emits(buf);
        emits("\033[0m");
        draw_shortcuts(ROWS - 2, (const char *const[][2]){{" Y", "Yes"}}, 1);
        draw_shortcuts(ROWS - 1, (const char *const[][2]){{" N", "No"}, {"^C", "Cancel"}}, 2);
        flush_out();
        int k = read_key();
        if (k == 'y' || k == 'Y') return 1;
        if (k == 'n' || k == 'N') return 0;
        if (k == 0x03) return -1;
    }
}

static void write_out(void) {
    char *name = prompt("File Name to Write", filename);
    if (!name || !name[0]) {
        snprintf(status, sizeof(status), "[ Cancelled ]");
        return;
    }
    snprintf(filename, sizeof(filename), "%s", name);
    save_file(filename);
}

static void search(void) {
    static char last[256];
    char *what = prompt(last[0] ? "Search" : "Search", last);
    if (!what || !what[0]) {
        snprintf(status, sizeof(status), "[ Cancelled ]");
        return;
    }
    snprintf(last, sizeof(last), "%s", what);
    int wl = strlen(what);
    for (int n = 0; n <= nlines; n++) {
        int li = (cy + n) % nlines;
        int start = (n == 0) ? cx + 1 : 0;
        line_t *l = &lines[li];
        for (int i = start; i + wl <= l->len; i++) {
            if (!strncmp(l->text + i, what, wl)) {
                cy = li;
                cx = i;
                status[0] = '\0';
                if (n == nlines) snprintf(status, sizeof(status), "[ This is the only occurrence ]");
                return;
            }
        }
    }
    snprintf(status, sizeof(status), "[ \"%s\" not found ]", what);
}

static void restore_terminal(void) {
    tcsetattr(STDIN_FILENO, TCSANOW, &saved_tty);
    write(STDOUT_FILENO, "\033[H\033[2J", 7);
}

int main(int argc, char *argv[]) {
    if (argc > 1) snprintf(filename, sizeof(filename), "%s", argv[argc - 1]);
    load_file(filename[0] ? filename : "/nonexistent/");
    if (!filename[0]) status[0] = '\0';

    tcgetattr(STDIN_FILENO, &saved_tty);
    struct termios raw = saved_tty;
    raw.c_lflag &= ~(ICANON | ECHO | ISIG);
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    emits("\033[H\033[2J");

    for (;;) {
        draw();
        int k = read_key();
        line_t *l = &lines[cy];
        if (k != -1 && k != 0x03) status[0] = '\0';

        switch (k) {
        case -1:
            restore_terminal();
            return 1;
        case 0x18: /* ^X */
            if (modified) {
                int r = ask_yes_no("Save modified buffer?  (Answering \"No\" will DISCARD changes.)");
                if (r < 0) {
                    snprintf(status, sizeof(status), "[ Cancelled ]");
                    break;
                }
                if (r == 1) {
                    char *name = prompt("File Name to Write", filename);
                    if (!name || !name[0]) {
                        snprintf(status, sizeof(status), "[ Cancelled ]");
                        break;
                    }
                    snprintf(filename, sizeof(filename), "%s", name);
                    if (save_file(filename) < 0) break;
                }
            }
            restore_terminal();
            return 0;
        case 0x0f: /* ^O */
            write_out();
            break;
        case 0x17: /* ^W */
            search();
            break;
        case 0x03: /* ^C */
            snprintf(status, sizeof(status), "[ line %d/%d (%d%%), col %d/%d ]", cy + 1, nlines,
                     (cy + 1) * 100 / nlines, cx + 1, l->len + 1);
            break;
        case 0x07: /* ^G */
            snprintf(status, sizeof(status), "^O save  ^X exit  ^K cut  ^U paste  ^W search  ^A/^E start/end");
            break;
        case 0x0b: /* ^K: cut the current line */
            free(cut_buffer);
            cut_buffer = malloc(l->len + 1);
            memcpy(cut_buffer, l->text, l->len);
            cut_buffer[l->len] = '\0';
            if (nlines > 1) delete_line(cy);
            else l->len = 0;
            if (cy >= nlines) cy = nlines - 1;
            cx = 0;
            modified = 1;
            break;
        case 0x15: /* ^U: paste above the current line */
            if (cut_buffer) {
                line_t *nl = insert_line(cy);
                line_insert(nl, 0, cut_buffer, strlen(cut_buffer));
                cy++;
                cx = 0;
                modified = 1;
            }
            break;
        case 0x01: case KEY_HOME: cx = 0; break;
        case 0x05: case KEY_END: cx = l->len; break;
        case KEY_UP:
            if (cy > 0) cy--;
            break;
        case KEY_DOWN:
            if (cy < nlines - 1) cy++;
            break;
        case KEY_LEFT:
            if (cx > 0) cx--;
            else if (cy > 0) { cy--; cx = lines[cy].len; }
            break;
        case KEY_RIGHT:
            if (cx < l->len) cx++;
            else if (cy < nlines - 1) { cy++; cx = 0; }
            break;
        case KEY_PGUP:
            cy = cy > EDIT_ROWS ? cy - EDIT_ROWS : 0;
            break;
        case KEY_PGDN:
            cy = cy + EDIT_ROWS < nlines ? cy + EDIT_ROWS : nlines - 1;
            break;
        case '\r':
        case '\n': {
            line_t *nl = insert_line(cy + 1);
            l = &lines[cy]; /* insert_line may have moved the array */
            line_insert(nl, 0, l->text + cx, l->len - cx);
            l->len = cx;
            cy++;
            cx = 0;
            modified = 1;
            break;
        }
        case 0x08:
        case 0x7f: /* backspace */
            if (cx > 0) {
                memmove(l->text + cx - 1, l->text + cx, l->len - cx);
                l->len--;
                cx--;
                modified = 1;
            } else if (cy > 0) {
                line_t *prev = &lines[cy - 1];
                cx = prev->len;
                line_insert(prev, prev->len, l->text, l->len);
                delete_line(cy);
                cy--;
                modified = 1;
            }
            break;
        case KEY_DEL:
            if (cx < l->len) {
                memmove(l->text + cx, l->text + cx + 1, l->len - cx - 1);
                l->len--;
                modified = 1;
            } else if (cy < nlines - 1) {
                line_insert(l, l->len, lines[cy + 1].text, lines[cy + 1].len);
                delete_line(cy + 1);
                modified = 1;
            }
            break;
        default:
            if (k == '\t' || (k >= 0x20 && k < 0x7f)) {
                char ch = (char)k;
                line_insert(l, cx, &ch, 1);
                cx++;
                modified = 1;
            }
            break;
        }
        if (cx > lines[cy].len) cx = lines[cy].len;
    }
}
