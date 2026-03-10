#include <ncurses.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_LINES 1000
#define MAX_LINE_LEN 1024

int main(int argc, char* argv[])
{
    if(argc != 2) {
        printf("Usage: %s <filename>\n", argv[0]);
        return 1;
    }

    FILE* file = fopen(argv[1], "r");
    char* buffer[MAX_LINES];
    int num_lines = 0;
    char line[MAX_LINE_LEN];

    if(file){
        while(fgets(line, sizeof(line), file) && num_lines < MAX_LINES){
            buffer[num_lines] = strdup(line);
            buffer[num_lines][strcspn(buffer[num_lines], "\n")] = 0;
            num_lines++;
        }
        fclose(file);
    }
    if(num_lines == 0) {
        buffer[0] = strdup("");
        num_lines = 1;
    }

    initscr();
    start_color();
    keypad(stdscr, TRUE);
    noecho();
    curs_set(1);

    init_pair(1, COLOR_BLACK, COLOR_CYAN);
    init_pair(2, COLOR_BLACK, COLOR_GREEN);

    int ch;
    int cursor_row = 0, cursor_col = 0;
    int leftPos = 0;

    // Header & footer texts
    char* headerMiddleText = "AK Editor";
    char* headerRightText = argv[1];
    char* footerLeftText = "Ctrl+Q to quit";
    char* footerMiddleText = "Arrow keys to move";
    char* footerRightText = "Ctrl+S to save";

    while(1)
    {
        int rows, cols;
        getmaxyx(stdscr, rows, cols);

        char headerLeftText[20];
        sprintf(headerLeftText, "Ln %d Col %d", cursor_row+1, cursor_col+1);

        int headerMiddlePos = (cols / 2) - (strlen(headerMiddleText)/2);
        int headerRightPos  = cols - strlen(headerRightText) - 1;
        int footerMiddlePos = (cols / 2) - (strlen(footerMiddleText)/2);
        int footerRightPos  = cols - strlen(footerRightText) - 1;

        clear();

        // Header
        attron(COLOR_PAIR(1) | A_BOLD | A_UNDERLINE);
        mvprintw(0, leftPos, "%s", headerLeftText);
        mvprintw(0, headerMiddlePos, "%s", headerMiddleText);
        mvprintw(0, headerRightPos, "%s", headerRightText);
        attroff(COLOR_PAIR(1) | A_BOLD | A_UNDERLINE);

        // Footer
        attron(COLOR_PAIR(2) | A_BOLD);
        mvprintw(rows-1, leftPos, "%s", footerLeftText);
        mvprintw(rows-1, footerMiddlePos, "%s", footerMiddleText);
        mvprintw(rows-1, footerRightPos, "%s", footerRightText);
        attroff(COLOR_PAIR(2) | A_BOLD);

        int start_row = 1;
        int max_rows = rows - 2;

        for(int i=0; i<num_lines && i<max_rows; i++)
            mvprintw(start_row + i, 0, "%s", buffer[i]);

        move(start_row + cursor_row, cursor_col);
        refresh();

        ch = getch();

        // Arrow key movements
        if(ch == KEY_UP && cursor_row > 0)
            cursor_row--;
        else if(ch == KEY_DOWN && cursor_row < num_lines -1 && cursor_row < max_rows-1)
            cursor_row++;
        else if(ch == KEY_LEFT && cursor_col > 0)
            cursor_col--;
        else if(ch == KEY_RIGHT) {
            int line_len = strlen(buffer[cursor_row]);
            if(cursor_col < line_len)
                cursor_col++;
        }

        // Typing printable chars
        else if(ch >= 32 && ch <= 126){
            int line_len = strlen(buffer[cursor_row]);
            for(int i=line_len; i>=cursor_col; i--)
                buffer[cursor_row][i+1] = buffer[cursor_row][i];
            buffer[cursor_row][cursor_col] = ch;
            cursor_col++;
        }

        // Backspace
        else if(ch == 127 || ch == KEY_BACKSPACE){
            if(cursor_col > 0){
                int line_len = strlen(buffer[cursor_row]);
                for(int i=cursor_col-1; i<line_len; i++)
                    buffer[cursor_row][i] = buffer[cursor_row][i+1];
                cursor_col--;
            }
        }

        // Enter / newline
        else if(ch == '\n' || ch == KEY_ENTER){
            char* new_line = strdup(buffer[cursor_row] + cursor_col);
            buffer[cursor_row][cursor_col] = '\0';
            for(int i=num_lines; i>cursor_row+1; i--)
                buffer[i] = buffer[i-1];
            buffer[cursor_row+1] = new_line;
            cursor_row++;
            cursor_col = 0;
            num_lines++;
        }

        else if(ch == 17)
            break;
        else if(ch == 19) {
            FILE* f = fopen(argv[1], "w");
            if(f) {
                for(int i = 0; i < num_lines; i++)
                    fprintf(f, "%s\n", buffer[i]);
                fclose(f);
            }
        }

        // Keep cursor in bounds
        int line_len = strlen(buffer[cursor_row]);
        if(cursor_col > line_len)
            cursor_col = line_len;
        if(cursor_row >= num_lines)
            cursor_row = num_lines-1;
    }

    // Free buffer
    for(int i=0; i<num_lines; i++)
        free(buffer[i]);

    endwin();
    return 0;
}