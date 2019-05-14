/****************************************************************
*	Author: Nicolas Leo                    						*
*	Class: CS 1550, 9/17/18                						*
*	Project 1: Double-Buffered Graphics Library					*
*	Description: This is a fix file in case the graphics app    *
*   crashes and echo is still turned off.
****************************************************************/
#include <sys/ioctl.h>

//Needed for termios struct & constants
#include <termios.h>
#include <unistd.h>

int main(int argc, char const *argv[])
{
	//open the frame buffer
	struct termios term;
	ioctl (0, TCGETS, &term); 			//Get current termios settings
	term.c_lflag |= (ECHO | ICANON); 	//Turn on echo & canonical mode
	ioctl (0, TCSETS, &term); 			//Set new termios settings

	write(1, "\033[2J", 4); 	// Clear the standard output (1)
	write(1,"\e[?25h", 6);      // display cursor
	return 0;
}