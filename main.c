#include <ncurses.h>

int main()
{
    initscr();
    start_color();
    keypad(stdscr, TRUE);

    init_pair(1, COLOR_BLACK, COLOR_CYAN);
    init_pair(2, COLOR_BLACK, COLOR_GREEN);

    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    int ch;

    while(1)
    {
        clear();

        attron(COLOR_PAIR(1));
        mvprintw(0, 0, " AK Editor ");
        attroff(COLOR_PAIR(1));

        attron(COLOR_PAIR(2));
        mvprintw(rows-1, 0, " Press q to quit ");
        attroff(COLOR_PAIR(2));

        refresh();

        ch = getch();

        if(ch == 'q')
            break;
    }

    endwin();
}