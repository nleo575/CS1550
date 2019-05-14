/****************************************************************
*	Author: Nicolas Leo                    						*
*	Class: CS 1550, 9/17/18                						*
*	Project 1: Double-Buffered Graphics Library					*
*	Description: Header file for library.c                      *
****************************************************************/

typedef unsigned short int color_t;
#define RGB(R, G, B) ((((R << 11))|(G << 5))|B)
void init_graphics();
void exit_graphics();
char getkey();
void sleep_ms(long ms);
void clear_screen(void *img);
void draw_pixel(void *img, int x, int y, color_t color);
void draw_line(void *img, int x1, int y1, int x2, int y2, color_t c); 
void *new_offscreen_buffer();
void blit(void *src);