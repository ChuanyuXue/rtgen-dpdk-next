/*

============ Source code ============
Link        : https://raw.githubusercontent.com/zoelabbb/conio.h/master/conio.h
GitHub      : https://github.com/zoelabbb/conio.h.git
=====================================

*/

/**
 * conio4linux (Conio.h for linux)
 * Copyright (C) 2013 by Alif Ryuu B R
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @author Alif Ryuu B R <zoelabbb@gmail.com>
 * @date 2023-12-21
 * @version 1.1
 */

/*
List funtions
=============
    cprintf
    cscanf
    gotoxy
    clrscr
    textcolor
    textbackground
    wherex
    wherey
    getch
    getche
    ungetch
    kbhit
    putch
    putchar
    cputs
    clreol
    insline (not implemented)
    delline (not implemented)
    cgets   (not implemented)
    getpass (not implemented)
    gettext (not implemented)
    _cprintf
    _cscanf
    _cputs
    _getche
    _kbhit
    _putch
    _ungetch

Instalation for linux
=====================
    Copy file conio.h to /usr/include/
    Run as root user:
      cp conio.h /usr/include/

Example program
===============
//File name: main.cpp
//Compilation: gcc main.cpp -o program_conio -lstdc++
//Test program: ./program_conio
#include<conio.h>
int main(int argc, char *argv[]){
  textbackground(BLUE);
  clrscr();
  textcolor(WHITE);
  gotoxy(30,5);
  cprintf("Hello World WHITE");
  textcolor(YELLOW);
  gotoxy(30,6);
  cprintf("Hello World YELLOW");
  for(int i=0;i<16;i++){
    textcolor(i);
    gotoxy(1,2+i);
    cprintf("Hello World");
    }
  getch();
  return 0;
  }

I need help for
===============
  *Document funtions.
  *Implement funtions not implemented.
  *Add funtions new.
  *Report bugs.

  For any case:
  Send a mail to zoelabbb@gmail.com with the modifications and information: Email, Autor and changes.
  Thanks!!!

*/
//#ifndef __CONIO4LINUX_H
//#define __CONIO4LINUX_H

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

#define BLACK 0
#define BLUE 1
#define GREEN 2
#define CYAN 3
#define RED 4
#define MAGENTA 5
#define BROWN 6
#define LIGHTGRAY 7
#define DARKGRAY 8
#define LIGHTBLUE 9
#define LIGHTGREEN 10
#define LIGHTCYAN 11
#define LIGHTRED 12
#define LIGHTMAGENTA 13
#define YELLOW 14
#define WHITE 15
#define BLINK 128

#define background_color 0



void clearLine();

void insertLine();

void deleteLine();

void setCursorPosition(int x, int y);

void clearScreen();

void setBackgroundColor(int color);

void setTextColor(short color);

int ungetch(int ch);

int setEchoMode(int enable);

int getch();

int getche();

/*int wherexy(int &x, int &y);

int wherex();

int wherey();*/

int kbhit();

int putch(const char c);

int cputs(const char *str);

/*char *getpass(const char *prompt);

int gettext(int l, int t, int r, int b, void *destination);*/