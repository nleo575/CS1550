/****************************************************************
*	Author: Nicolas Leo                    						*
*	Class: CS 1550, 9/17/18                						*
*	Project 1: Double-Buffered Graphics Library					*
*	Description: Basic graphics library implementation          *
*   No known errors & works perfectly with hilbert.c file.      *
****************************************************************/



#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h> 	//Needed for mmap & munmap functions
#include <sys/ioctl.h>
#include <linux/fb.h> 	//needed for fbvar & fbfix structs
#include <sys/select.h> //needed to check if a device is ready w/out blocking
#include <termios.h>	//Needed for termios struct & constants
#include <unistd.h>
#include <time.h> 		// Needed for timespec struct

typedef unsigned short color_t;

static unsigned char initialized = 0; 	// Indicates if the graphics are initialized

static char key = 0; 					// Holds a single character input from the keyboard

static int fbuff_fd, 					// Frame buffer file descriptor 
		   kb,							// Keyboard file descriptor
		   xres,						// Virtual xres
		   yres;						// Virtual yres

static long sizetommap; 				// Total size of the off-screen buffer in bytes

static void * fbuff; 					// Address of mmapped off-screen buffer
static void * osbuff = NULL; 			// Address of mmapped off-screen buffer

static struct fb_var_screeninfo fbvar;	// Frame buffer variables
static struct fb_fix_screeninfo fbfix;	// Frame buffer variables
static struct termios term;				// Needed for terminal settings

void init_graphics()
{
	fbuff_fd = open ("/dev/fb0", O_RDWR);	//open the frame buffer

	ioctl (fbuff_fd, FBIOGET_VSCREENINFO, &fbvar); // Get current fbvar for y-resolution
	ioctl (fbuff_fd, FBIOGET_FSCREENINFO, &fbfix); // Get current fbfix for bit depth

	xres = fbvar.xres_virtual;
	yres = fbvar.yres_virtual;

	//lines*bytes/line = byte size off-screen buffer
	sizetommap = yres * fbfix.line_length; // In bytes

	//Create off-screen buffer
	fbuff = mmap(NULL, sizetommap, PROT_READ|PROT_WRITE, MAP_SHARED, fbuff_fd, 0);
	write(1, "\e[?25l", 6);  	// hide cursor
	write(1, "\033[2J", 4); 	// Clear the standard output (1)

	ioctl (0, TCGETS, &term); 	//Get current termios settings
	term.c_lflag &= ~(ECHO | ICANON);	//Turn off echo & canonical mode
	ioctl (0, TCSETS, &term); 	//Set new termios settings

	initialized = 1; //Used later to signal that initialization complete
}

void exit_graphics()
{
	if (initialized) // Only do the following if init_graphics successfully called
	{
		write(1, "\033[2J", 4); 	// Clear the standard output (1)
		write(1,"\e[?25h", 6);      // display cursor

		term.c_lflag |= (ECHO | ICANON); //Turn on echo & canonical mode
		ioctl (0, TCSETS, &term); //Set new termios settings

		munmap(fbuff, sizetommap); 		 // Delete memory mapping from initialization 

		// Delete memory mapping from new_offscreen_buffer
		if (osbuff != NULL) munmap(osbuff, sizetommap); 

		close(fbuff_fd); 			// Close the frame buffer
		initialized = 0;
	}
} 
 

/*
	This function gets a single key press from standard input (0)
*/
char getkey()
{
	if (initialized)
	{
		static fd_set rfds;
		static struct timeval tv;

      	// Watch stdin (fd 0) to see when it has input.
        FD_ZERO(&rfds);
        FD_SET(0, &rfds); // 0 is stdin
	
		tv.tv_sec = 0;
		tv.tv_usec = 0;
		// Read only if there is a key press ready
		if (select(1, &rfds, NULL, NULL, &tv) > 0 && read(0, &key, 1) > 0) return key; 
	}
	return 0;
}

void sleep_ms(long ms)
{ 
	if (ms < 1000 && ms > 0) // tv_nsec has a max value of 999,999,999
	{
		static struct timespec tspec;
		
		tspec.tv_sec = 0;
		tspec.tv_nsec = ms * 1000000L;

		nanosleep (&tspec, NULL);
	}
 }
 
void clear_screen(void *img)
{	// Check for initialization & valid arguments
	if (initialized && img != NULL)
	{
		static int i;
		for (i = 0; i < sizetommap; ++i)
			*(((char *) img)+i) = 0;
	}
}

void draw_pixel(void *img, int x, int y, color_t color)
{	// Check for initialization & valid arguments
	if (initialized && img != NULL && x > -1 && x < xres && y> -1 && y < yres)
		*((color_t *) img + x + y*xres) = color;
}
 
void draw_line(void *img, int x1, int y1, int x2, int y2, color_t c)
{
	// Check for initialization & valid arguments
	if (initialized && img!=NULL && x1 > -1 && x1 < xres && x2 > -1 && x2 <= xres &&
					   				y1 > -1 && y1 < yres && y2 > -1 && y2 <= yres)
	{
		// Implementation of Bresenham's Line Plotting Algorithm
		// Source: https://gist.github.com/bert/1085538
		int dx =  abs (x2 - x1), 
			sx = x1 < x2 ? 1 : -1,
			dy = -abs (y2 - y1), 
			sy = y1 < y2 ? 1 : -1,
			err = dx + dy, 
			e2; /* error value e_xy */

		while(1)
		{ 
			*((color_t *) img + x1 + (y1*xres)) = c; // Draw the pixel
			if (x1 == x2 && y1 == y2) break; // All pixels drawn
			e2 = err << 1; //2 * err
			if (e2 >= dy) { err += dy; x1 += sx; } /* e_xy+e_x > 0 */
			if (e2 <= dx) { err += dx; y1 += sy; } /* e_xy+e_y < 0 */
		}
	}
} 
 
 
void *new_offscreen_buffer()
{
	if (initialized)
	{
		osbuff = mmap(NULL, sizetommap, PROT_READ|PROT_WRITE, 
			MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
		if(osbuff == MAP_FAILED) return NULL;
	}

	return osbuff;
}

void blit(void *src)
{
	if (initialized && src != NULL)
	{
		static int i;
		for (i = 0; i < sizetommap; ++i)
			*(((char *) fbuff)+i) = *(((char*)src)+i);
	}
}