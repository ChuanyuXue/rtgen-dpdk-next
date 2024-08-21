#include "conio.h"

void clearLine()
{
    printf("\033[K");
}

void insertLine()
{
    printf("\x1b[1L");
}

void deleteLine()
{
    printf("\033[1M");
}

void setCursorPosition(int x, int y)
{
    printf("\033[%d;%df", y, x);
}

void clearScreen()
{
    printf("\033[%dm\033[2J\033[1;1f", background_color);
}

void setBackgroundColor(int color)
{
    //background_color = (color % 16) + 40;
}

void setTextColor(short color)
{
    if (color >= 0 && color < 16)
    {
        printf("\033[0;%d;%dm", 30 + color, background_color);
    }
}

int ungetch(int ch)
{
    return ungetc(ch, stdin);
}

int setEchoMode(int enable)
{
    struct termios oldt, newt;
    int ch;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~ICANON;
    if (enable)
        newt.c_lflag |= ECHO;
    else
        newt.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return ch;
}

int getch()
{
    return setEchoMode(0);
}

int getche()
{
    return setEchoMode(1);
}

/*int wherexy(int &x, int &y)
{
    printf("\033[6n");
    if (getch() != '\x1B')
        return 0;
    if (getch() != '\x5B')
        return 0;
    int in;
    int ly = 0;
    while ((in = getch()) != ';')
        ly = ly * 10 + in - '0';
    int lx = 0;
    while ((in = getch()) != 'R')
        lx = lx * 10 + in - '0';
    x = lx;
    y = ly;
}

int wherex()
{
    int x = 0, y = 0;
    wherexy(x, y);
    return x;
}

int wherey()
{
    int x = 0, y = 0;
    wherexy(x, y);
    return y;
}*/

int kbhit()
{
    struct termios oldt, newt;
    int ch;
    int oldf;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);
    if (ch != EOF)
    {
        ungetc(ch, stdin);
        return 1;
    }
    return 0;
}

int putch(const char c)
{
    printf("%c", c);
    return (int)c;
}

int cputs(const char *str)
{
    printf("%s", str);
    return 0;
}