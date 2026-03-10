#include <ncurses.h>
#include <string.h>

int main()
{
    initscr();
    start_color();
    keypad(stdscr, TRUE);
    noecho();
    curs_set(0);

    init_pair(1, COLOR_BLACK, COLOR_CYAN);
    init_pair(2, COLOR_BLACK, COLOR_GREEN);

    int ch;
    int leftPos = 0;

    // Header texts
    char* headerLeftText = "Ln 12 Col 5";
    char* headerMiddleText = "lucrativeText";
    char* headerRightText = "filename.txt";

    // Footer texts
    char* footerLeftText = "Ctrl+Q to quit";
    char* footerMiddleText = "Ctrl+S to save";
    char* footerRightText = "Ctrl+W to close without save";

    while(1)
    {
        int rows, cols;
        getmaxyx(stdscr, rows, cols);

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

        refresh();

        ch = getch();

        if(ch == 17)
            break;

        if(ch == 18)
            continue;
    }

    endwin();
    return 0;
}