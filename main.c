#include <ncurses.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#define INIT_LINES 64
#define INIT_LINE_LEN 64
#define UNDO_LIMIT 128

char** buffer = NULL;
int num_lines = 0;
int buffer_cap = 0;
int dirty = 0;

typedef struct {
    char** lines;
    int num_lines;
    int cursor_row;
    int cursor_col;
} Snapshot;

Snapshot undo_stack[UNDO_LIMIT];
int undo_top = 0;

void ensure_line_cap(int row, int needed)
{
    int cur_cap = strlen(buffer[row]) + 1;
    if(needed <= cur_cap) return;
    while(cur_cap <= needed) cur_cap *= 2;
    buffer[row] = realloc(buffer[row], cur_cap);
}

void ensure_buffer_lines(int needed)
{
    if(needed <= buffer_cap) return;
    while(buffer_cap <= needed) buffer_cap *= 2;
    buffer = realloc(buffer, buffer_cap * sizeof(char*));
}

void push_undo(int crow, int ccol)
{
    Snapshot* s = &undo_stack[undo_top % UNDO_LIMIT];
    if(s->lines) {
        for(int i = 0; i < s->num_lines; i++) free(s->lines[i]);
        free(s->lines);
    }
    s->lines = malloc(num_lines * sizeof(char*));
    for(int i = 0; i < num_lines; i++) s->lines[i] = strdup(buffer[i]);
    s->num_lines = num_lines;
    s->cursor_row = crow;
    s->cursor_col = ccol;
    undo_top++;
}

int pop_undo(int* crow, int* ccol)
{
    if(undo_top == 0) return 0;
    undo_top--;
    Snapshot* s = &undo_stack[undo_top % UNDO_LIMIT];
    if(!s->lines) return 0;
    for(int i = 0; i < num_lines; i++) free(buffer[i]);
    buffer_cap = s->num_lines + 1;
    buffer = realloc(buffer, buffer_cap * sizeof(char*));
    num_lines = s->num_lines;
    for(int i = 0; i < num_lines; i++) buffer[i] = strdup(s->lines[i]);
    *crow = s->cursor_row;
    *ccol = s->cursor_col;
    return 1;
}

int save_file(const char* path)
{
    FILE* f = fopen(path, "w");
    if(!f) return 0;
    for(int i = 0; i < num_lines; i++)
        fprintf(f, "%s\n", buffer[i]);
    fclose(f);
    return 1;
}

void show_status(int rows, int cols, const char* msg, int color_pair)
{
    attron(COLOR_PAIR(color_pair) | A_BOLD);
    mvprintw(rows - 1, 0, "%*s", cols - 1, "");
    int pos = (cols / 2) - (strlen(msg) / 2);
    mvprintw(rows - 1, pos, "%s", msg);
    attroff(COLOR_PAIR(color_pair) | A_BOLD);
    refresh();
}

int count_leading_spaces(const char* line)
{
    int n = 0;
    while(line[n] == ' ') n++;
    return n;
}

void normalize_sel(int sr, int sc, int er, int ec,
                   int* out_sr, int* out_sc, int* out_er, int* out_ec)
{
    if(sr < er || (sr == er && sc <= ec)) {
        *out_sr = sr; *out_sc = sc; *out_er = er; *out_ec = ec;
    } else {
        *out_sr = er; *out_sc = ec; *out_er = sr; *out_ec = sc;
    }
}

char* sel_to_str(int sr, int sc, int er, int ec)
{
    int total = 0;
    for(int r = sr; r <= er; r++) {
        int from = (r == sr) ? sc : 0;
        int to   = (r == er) ? ec : (int)strlen(buffer[r]);
        total += to - from + 1;
    }
    char* out = malloc(total + 1);
    int pos = 0;
    for(int r = sr; r <= er; r++) {
        int from = (r == sr) ? sc : 0;
        int to   = (r == er) ? ec : (int)strlen(buffer[r]);
        for(int c = from; c < to; c++) out[pos++] = buffer[r][c];
        if(r < er) out[pos++] = '\n';
    }
    out[pos] = '\0';
    return out;
}

void delete_selection(int sr, int sc, int er, int ec)
{
    if(sr == er) {
        int len = strlen(buffer[sr]);
        memmove(buffer[sr] + sc, buffer[sr] + ec, len - ec + 1);
    } else {
        int tail_len = strlen(buffer[er]) - ec;
        ensure_line_cap(sr, sc + tail_len + 1);
        buffer[sr][sc] = '\0';
        strncat(buffer[sr], buffer[er] + ec, tail_len);
        for(int i = sr + 1; i <= er; i++) free(buffer[i]);
        int removed = er - sr;
        for(int i = sr + 1; i < num_lines - removed; i++)
            buffer[i] = buffer[i + removed];
        num_lines -= removed;
    }
}

int main(int argc, char* argv[])
{
    if(argc != 2) {
        printf("Usage: %s <filename>\n", argv[0]);
        return 1;
    }

    int file_existed = 0;
    buffer_cap = INIT_LINES;
    buffer = malloc(buffer_cap * sizeof(char*));
    memset(undo_stack, 0, sizeof(undo_stack));

    FILE* file = fopen(argv[1], "r");
    if(file) {
        file_existed = 1;
        char* line = malloc(INIT_LINE_LEN);
        int line_cap = INIT_LINE_LEN;
        int c, pos = 0;
        while((c = fgetc(file)) != EOF) {
            if(pos + 2 >= line_cap) { line_cap *= 2; line = realloc(line, line_cap); }
            if(c == '\n') {
                line[pos] = '\0';
                ensure_buffer_lines(num_lines + 1);
                buffer[num_lines++] = strdup(line);
                pos = 0;
            } else {
                line[pos++] = c;
            }
        }
        if(pos > 0) {
            line[pos] = '\0';
            ensure_buffer_lines(num_lines + 1);
            buffer[num_lines++] = strdup(line);
        }
        free(line);
        fclose(file);
    }

    if(num_lines == 0) {
        ensure_buffer_lines(1);
        buffer[num_lines++] = strdup("");
    }

    initscr();
    start_color();
    keypad(stdscr, TRUE);
    noecho();
    curs_set(1);

    init_pair(1, COLOR_BLACK, COLOR_CYAN);
    init_pair(2, COLOR_BLACK, COLOR_GREEN);
    init_pair(3, COLOR_BLACK, COLOR_YELLOW);
    init_pair(4, COLOR_WHITE, COLOR_RED);
    init_pair(5, COLOR_BLACK, COLOR_YELLOW);
    init_pair(6, COLOR_BLACK, COLOR_WHITE);

    int ch;
    int cursor_row = 0, cursor_col = 0;
    int top_row = 0, view_col = 0;

    int sel_active = 0;
    int sel_start_row = 0, sel_start_col = 0;
    int sel_end_row = 0, sel_end_col = 0;

    char* clipboard = strdup("");

    char status_msg[128] = "";
    int status_color = 2;
    int status_timer = 0;

    int show_gutter = 1;
    char* headerMiddleText = "lucrativeText Editor";

    while(1)
    {
        int rows, cols;
        getmaxyx(stdscr, rows, cols);

        int gutter = show_gutter ? 4 : 0;
        int text_cols = cols - gutter;
        int start_row = 1;
        int max_rows = rows - 2;

        clear();

        int nsr, nsc, ner, nec;
        if(sel_active)
            normalize_sel(sel_start_row, sel_start_col,
                          sel_end_row, sel_end_col,
                          &nsr, &nsc, &ner, &nec);

        char header_left[64];
        sprintf(header_left, " Ln %d Col %d%s", cursor_row + 1, cursor_col + 1, dirty ? " [+]" : "");
        char* header_right = argv[1];

        int headerMiddlePos = (cols / 2) - (strlen(headerMiddleText) / 2);
        int headerRightPos  = cols - strlen(header_right) - 1;

        attron(COLOR_PAIR(1) | A_BOLD);
        mvprintw(0, 0, "%*s", cols, "");
        mvprintw(0, 0, "%s", header_left);
        mvprintw(0, headerMiddlePos, "%s", headerMiddleText);
        mvprintw(0, headerRightPos, "%s", header_right);
        attroff(COLOR_PAIR(1) | A_BOLD);

        attron(COLOR_PAIR(2) | A_BOLD);
        mvprintw(rows - 1, 0, "%*s", cols, "");
        const char* footer_items[] = {
            "^E exit", "^W close", "^X save", "^U undo",
            "^D del", "^K cut", "^P paste", "^G copy", "^N nums"
        };
        int footer_count = 9;
        int spacing = cols / footer_count;
        for(int fi = 0; fi < footer_count; fi++)
            mvprintw(rows - 1, fi * spacing + (spacing / 2) - (strlen(footer_items[fi]) / 2), "%s", footer_items[fi]);
        attroff(COLOR_PAIR(2) | A_BOLD);

        if(status_timer > 0) {
            show_status(rows, cols, status_msg, status_color);
            status_timer--;
        }

        for(int i = 0; i < max_rows && (top_row + i) < num_lines; i++) {
            int buf_row = top_row + i;
            int screen_row = start_row + i;
            char* line = buffer[buf_row];
            int len = strlen(line);

            if(buf_row == cursor_row) {
                attron(COLOR_PAIR(6));
                mvprintw(screen_row, 0, "%*s", cols, "");
                attroff(COLOR_PAIR(6));
            }

            if(show_gutter) {
                attron(A_DIM | (buf_row == cursor_row ? COLOR_PAIR(6) : 0));
                mvprintw(screen_row, 0, "%3d ", buf_row + 1);
                attroff(A_DIM | COLOR_PAIR(6));
            }

            for(int c2 = view_col; c2 < len && (c2 - view_col) < text_cols; c2++) {
                int in_sel = sel_active
                    && (buf_row > nsr || (buf_row == nsr && c2 >= nsc))
                    && (buf_row < ner || (buf_row == ner && c2 < nec));

                if(in_sel)
                    attron(COLOR_PAIR(5) | A_BOLD);
                else if(buf_row == cursor_row)
                    attron(COLOR_PAIR(6));
                else
                    attroff(0xffff);

                mvprintw(screen_row, gutter + (c2 - view_col), "%c", line[c2]);
                attroff(COLOR_PAIR(5) | A_BOLD | COLOR_PAIR(6));
            }

            if(buf_row == cursor_row && !sel_active) {
                attron(COLOR_PAIR(6));
                for(int c2 = len - view_col; c2 < text_cols; c2++)
                    mvprintw(screen_row, gutter + c2, " ");
                attroff(COLOR_PAIR(6));
            }
        }

        move(start_row + (cursor_row - top_row), gutter + cursor_col - view_col);
        refresh();

        ch = getch();

        int is_shift_arrow = (ch == KEY_SR || ch == KEY_SF ||
                              ch == KEY_SLEFT || ch == KEY_SRIGHT);
        if(is_shift_arrow) {
            if(!sel_active) {
                sel_active = 1;
                sel_start_row = cursor_row;
                sel_start_col = cursor_col;
            }
            if(ch == KEY_SR && cursor_row > 0)       { cursor_row--; view_col = 0; }
            else if(ch == KEY_SF && cursor_row < num_lines - 1) { cursor_row++; view_col = 0; }
            else if(ch == KEY_SLEFT && cursor_col > 0) {
                cursor_col--;
                if(cursor_col < view_col) view_col = cursor_col;
            }
            else if(ch == KEY_SRIGHT) {
                int len = strlen(buffer[cursor_row]);
                if(cursor_col < len) {
                    cursor_col++;
                    if(cursor_col - view_col >= text_cols) view_col++;
                }
            }
            sel_end_row = cursor_row;
            sel_end_col = cursor_col;
        }

        else if(ch == KEY_UP && cursor_row > 0)
            { sel_active = 0; cursor_row--; view_col = 0; }
        else if(ch == KEY_DOWN && cursor_row < num_lines - 1)
            { sel_active = 0; cursor_row++; view_col = 0; }
        else if(ch == KEY_LEFT && cursor_col > 0) {
            sel_active = 0;
            cursor_col--;
            if(cursor_col < view_col) view_col = cursor_col;
        }
        else if(ch == KEY_RIGHT) {
            sel_active = 0;
            int len = strlen(buffer[cursor_row]);
            if(cursor_col < len) {
                cursor_col++;
                if(cursor_col - view_col >= text_cols) view_col++;
            }
        }
        else if(ch == KEY_HOME) { sel_active = 0; cursor_col = 0; view_col = 0; }
        else if(ch == KEY_END) {
            sel_active = 0;
            cursor_col = strlen(buffer[cursor_row]);
            if(cursor_col - view_col >= text_cols)
                view_col = cursor_col - text_cols + 1;
        }
        else if(ch == KEY_PPAGE) {
            sel_active = 0;
            cursor_row -= max_rows;
            if(cursor_row < 0) cursor_row = 0;
            view_col = 0;
        }
        else if(ch == KEY_NPAGE) {
            sel_active = 0;
            cursor_row += max_rows;
            if(cursor_row >= num_lines) cursor_row = num_lines - 1;
            view_col = 0;
        }

        else if(ch == 27) {
            nodelay(stdscr, TRUE);
            int c2 = getch();
            nodelay(stdscr, FALSE);
            if(c2 == 'b') {
                sel_active = 0;
                while(cursor_col > 0 && !isalnum(buffer[cursor_row][cursor_col-1])) cursor_col--;
                while(cursor_col > 0 && isalnum(buffer[cursor_row][cursor_col-1]))  cursor_col--;
                if(cursor_col < view_col) view_col = cursor_col;
            } else if(c2 == 'f') {
                sel_active = 0;
                int len = strlen(buffer[cursor_row]);
                while(cursor_col < len && !isalnum(buffer[cursor_row][cursor_col])) cursor_col++;
                while(cursor_col < len &&  isalnum(buffer[cursor_row][cursor_col])) cursor_col++;
                if(cursor_col - view_col >= text_cols) view_col = cursor_col - text_cols + 1;
            }
        }

        else if(ch == '\t') {
            push_undo(cursor_row, cursor_col);
            dirty = 1; sel_active = 0;
            int spaces = 4 - (cursor_col % 4);
            for(int s = 0; s < spaces; s++) {
                int len = strlen(buffer[cursor_row]);
                ensure_line_cap(cursor_row, len + 2);
                for(int i = len; i >= cursor_col; i--)
                    buffer[cursor_row][i+1] = buffer[cursor_row][i];
                buffer[cursor_row][cursor_col] = ' ';
                cursor_col++;
                if(cursor_col - view_col >= text_cols) view_col++;
            }
        }
        else if(ch == KEY_BTAB) {
            push_undo(cursor_row, cursor_col);
            dirty = 1; sel_active = 0;
            int spaces = count_leading_spaces(buffer[cursor_row]);
            int rem = spaces % 4 == 0 ? 4 : spaces % 4;
            if(rem > spaces) rem = spaces;
            if(rem > 0) {
                int len = strlen(buffer[cursor_row]);
                memmove(buffer[cursor_row], buffer[cursor_row] + rem, len - rem + 1);
                cursor_col -= rem;
                if(cursor_col < 0) cursor_col = 0;
                if(cursor_col < view_col) view_col = cursor_col;
            }
        }

        else if(ch >= 32 && ch <= 126) {
            push_undo(cursor_row, cursor_col);
            dirty = 1;
            if(sel_active) {
                normalize_sel(sel_start_row, sel_start_col,
                              sel_end_row, sel_end_col,
                              &nsr, &nsc, &ner, &nec);
                delete_selection(nsr, nsc, ner, nec);
                cursor_row = nsr; cursor_col = nsc;
                sel_active = 0;
            }
            int len = strlen(buffer[cursor_row]);
            ensure_line_cap(cursor_row, len + 2);
            for(int i = len; i >= cursor_col; i--)
                buffer[cursor_row][i+1] = buffer[cursor_row][i];
            buffer[cursor_row][cursor_col] = ch;
            cursor_col++;
            if(cursor_col - view_col >= text_cols) view_col++;
        }

        else if(ch == 127 || ch == KEY_BACKSPACE) {
            push_undo(cursor_row, cursor_col);
            dirty = 1;
            if(sel_active) {
                normalize_sel(sel_start_row, sel_start_col,
                              sel_end_row, sel_end_col,
                              &nsr, &nsc, &ner, &nec);
                delete_selection(nsr, nsc, ner, nec);
                cursor_row = nsr; cursor_col = nsc;
                sel_active = 0;
            } else if(cursor_col > 0) {
                int len = strlen(buffer[cursor_row]);
                memmove(buffer[cursor_row] + cursor_col - 1,
                        buffer[cursor_row] + cursor_col,
                        len - cursor_col + 1);
                cursor_col--;
                if(cursor_col < view_col) view_col = cursor_col;
            } else if(cursor_row > 0) {
                int prev_len = strlen(buffer[cursor_row - 1]);
                int curr_len = strlen(buffer[cursor_row]);
                ensure_line_cap(cursor_row - 1, prev_len + curr_len + 1);
                strcat(buffer[cursor_row - 1], buffer[cursor_row]);
                free(buffer[cursor_row]);
                for(int i = cursor_row; i < num_lines - 1; i++)
                    buffer[i] = buffer[i+1];
                num_lines--;
                cursor_row--;
                cursor_col = prev_len;
                view_col = 0;
            }
        }

        else if(ch == '\n' || ch == KEY_ENTER) {
            push_undo(cursor_row, cursor_col);
            dirty = 1;
            if(sel_active) {
                normalize_sel(sel_start_row, sel_start_col,
                              sel_end_row, sel_end_col,
                              &nsr, &nsc, &ner, &nec);
                delete_selection(nsr, nsc, ner, nec);
                cursor_row = nsr; cursor_col = nsc;
                sel_active = 0;
            }
            int indent = count_leading_spaces(buffer[cursor_row]);
            ensure_buffer_lines(num_lines + 1);
            char* rest = strdup(buffer[cursor_row] + cursor_col);
            buffer[cursor_row][cursor_col] = '\0';
            for(int i = num_lines; i > cursor_row + 1; i--)
                buffer[i] = buffer[i-1];
            char* new_line = malloc(indent + strlen(rest) + 1);
            memset(new_line, ' ', indent);
            strcpy(new_line + indent, rest);
            free(rest);
            buffer[cursor_row + 1] = new_line;
            num_lines++;
            cursor_row++;
            cursor_col = indent;
            view_col = 0;
        }

        else if(ch == ('g' & 0x1f)) {
            if(sel_active) {
                normalize_sel(sel_start_row, sel_start_col,
                              sel_end_row, sel_end_col,
                              &nsr, &nsc, &ner, &nec);
                free(clipboard);
                clipboard = sel_to_str(nsr, nsc, ner, nec);
                strcpy(status_msg, "  Copied  ");
                status_color = 3;
                status_timer = 6;
            }
        }

        else if(ch == ('k' & 0x1f)) {
            push_undo(cursor_row, cursor_col);
            dirty = 1;
            if(sel_active) {
                normalize_sel(sel_start_row, sel_start_col,
                              sel_end_row, sel_end_col,
                              &nsr, &nsc, &ner, &nec);
                free(clipboard);
                clipboard = sel_to_str(nsr, nsc, ner, nec);
                delete_selection(nsr, nsc, ner, nec);
                cursor_row = nsr; cursor_col = nsc;
                sel_active = 0;
            } else {
                free(clipboard);
                clipboard = strdup(buffer[cursor_row]);
                free(buffer[cursor_row]);
                for(int i = cursor_row; i < num_lines - 1; i++)
                    buffer[i] = buffer[i+1];
                if(num_lines > 1) num_lines--;
                else buffer[0] = strdup("");
                if(cursor_row >= num_lines) cursor_row = num_lines - 1;
                cursor_col = 0; view_col = 0;
            }
            strcpy(status_msg, "  Cut  ");
            status_color = 3;
            status_timer = 6;
        }

        else if(ch == ('p' & 0x1f)) {
            if(clipboard && strlen(clipboard) > 0) {
                push_undo(cursor_row, cursor_col);
                dirty = 1; sel_active = 0;
                char* p = clipboard;
                while(*p) {
                    if(*p == '\n') {
                        ensure_buffer_lines(num_lines + 1);
                        char* rest = strdup(buffer[cursor_row] + cursor_col);
                        buffer[cursor_row][cursor_col] = '\0';
                        for(int i = num_lines; i > cursor_row + 1; i--)
                            buffer[i] = buffer[i-1];
                        buffer[cursor_row + 1] = rest;
                        num_lines++;
                        cursor_row++;
                        cursor_col = 0;
                        view_col = 0;
                    } else {
                        int len = strlen(buffer[cursor_row]);
                        ensure_line_cap(cursor_row, len + 2);
                        for(int i = len; i >= cursor_col; i--)
                            buffer[cursor_row][i+1] = buffer[cursor_row][i];
                        buffer[cursor_row][cursor_col] = *p;
                        cursor_col++;
                        if(cursor_col - view_col >= text_cols) view_col++;
                    }
                    p++;
                }
                strcpy(status_msg, "  Pasted  ");
                status_color = 3;
                status_timer = 6;
            }
        }

        else if(ch == ('n' & 0x1f)) {
            show_gutter = !show_gutter;
        }
        else if(ch == ('d' & 0x1f)) {
            push_undo(cursor_row, cursor_col);
            dirty = 1; sel_active = 0;
            free(buffer[cursor_row]);
            for(int i = cursor_row; i < num_lines - 1; i++)
                buffer[i] = buffer[i+1];
            if(num_lines > 1) num_lines--;
            else buffer[0] = strdup("");
            if(cursor_row >= num_lines) cursor_row = num_lines - 1;
            cursor_col = 0; view_col = 0;
        }

        else if(ch == ('u' & 0x1f)) {
            int new_row = cursor_row, new_col = cursor_col;
            if(pop_undo(&new_row, &new_col)) {
                cursor_row = new_row; cursor_col = new_col;
                dirty = 1; sel_active = 0; view_col = 0;
                strcpy(status_msg, "  Undone  ");
                status_color = 3; status_timer = 6;
            }
        }

        else if(ch == ('x' & 0x1f)) {
            if(save_file(argv[1])) {
                dirty = 0;
                strcpy(status_msg, "  Saved successfully!  ");
                status_color = 2;
            } else {
                strcpy(status_msg, "  Error: could not save file!  ");
                status_color = 4;
            }
            status_timer = 8;
        }

        else if(ch == ('e' & 0x1f)) {
            int rows2, cols2;
            getmaxyx(stdscr, rows2, cols2);
            if(dirty)
                show_status(rows2, cols2, "  Unsaved changes! Press Ctrl+E again to exit  ", 4);
            else
                show_status(rows2, cols2, "  Press Ctrl+E again to exit  ", 3);
            int ans = getch();
            if(ans == ('e' & 0x1f)) {
                if(dirty) save_file(argv[1]);
                break;
            }
        }

        else if(ch == ('w' & 0x1f)) {
            if(!file_existed) { break; }
            int rows2, cols2;
            getmaxyx(stdscr, rows2, cols2);
            show_status(rows2, cols2, "  Close without saving? (y/n)  ", 4);
            int ans = getch();
            if(ans == 'y' || ans == 'Y') break;
        }

        int line_len = strlen(buffer[cursor_row]);
        if(cursor_col > line_len) cursor_col = line_len;
        if(cursor_row >= num_lines) cursor_row = num_lines - 1;
        if(cursor_row >= top_row + max_rows) top_row = cursor_row - max_rows + 1;
        if(cursor_row < top_row) top_row = cursor_row;
    }

    for(int i = 0; i < num_lines; i++) free(buffer[i]);
    free(buffer);
    free(clipboard);
    for(int i = 0; i < UNDO_LIMIT; i++) {
        if(undo_stack[i].lines) {
            for(int j = 0; j < undo_stack[i].num_lines; j++) free(undo_stack[i].lines[j]);
            free(undo_stack[i].lines);
        }
    }

    endwin();
    return 0;
}