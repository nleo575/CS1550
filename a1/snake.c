/****************************************************************
*	Author: Nicolas Leo                    						*
*	Class: CS 1550, 9/17/18                						*
*	Project 1: Double-Buffered Graphics Library					*
*	Description: Simple snake game to test the graphics library	*
*   There are still a few bug (segmentation faults) but a decent*
*   Demo to see the functions in action.                        *
****************************************************************/


// There were issues in the original file with food(), draw_box, and when the user pressed 
// the left arrow key to move to the left. All problems have been fixed. 
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include "graphics.h"

#define SIZE 80 // Works best with 1, 5, 10, 20, 40, 80, or 160
//at this resolution
//8x6

typedef struct
{
	int index;
	struct Node *next;
} Node;

// Linked list implementation to keep track of & change cells on the board
// Works in conjunction with the char array board.
static Node *firstNode = NULL;
static Node *lastNode = NULL;
static Node *tempN = NULL;
static int length = 1;

static int maxIndex; // Used to keep track fo the board's index

static void addNode(int, int , char *, void *);
static void draw_box(void *, int, int, int, color_t);
static void draw_bg(void *, color_t);

static int boardxmax, boardymax; // Max tiles for a given resolution (#defined SIZE)
static void food(void*, int, int, int, char *); // Puts food on the board for the snake


int main(int argc, char const *argv[])
{
	write(1, "\033[H\033[2J", 7); 	// Clear the standard output (1)
	puts("Welcome to Snake!");
	puts("On the next screen, press any arrow key to get started.");
	puts("Hit any 'q' to exit the game at any time.");
	puts("These directions will close in 10 seconds.");

	// sleep(10);						// Let the user read the instructions
	init_graphics();
	srand((unsigned int)time(0)); 	//Seed the random # generator, used for making food
	unsigned char move = 1;			// Keeps track of the user's last move

	
	void *buf = new_offscreen_buffer(); //Construct an off-screen buffer to draw to

	static char key;
	// Holds individual key presses using the getkey() function Non-blocking

	static char ikey [3]= {0, 0, 0}; 
	//used for initial read to block the program from progressing
	// until the user presses an arrow key
	
	boardxmax = 640/SIZE; //Used for offsetting printing of the blocks of pixels
	boardymax = 480/SIZE;


	// Calculate how many blocks can fit on the screen at this resolutions
	int t = (int)((640.0*480.0)/(SIZE *SIZE)); // Temp variable for calculations

	maxIndex = t;
	char board [t]; // Used to keep track of legal spaces on the board. 

	int i; 
	for (i = 0; i < t; ++i)
		board[i] = 1;

	//Snake starts out in the middle of the board
	int head = (t/2 + ((((t/2)%boardxmax )== 0 )? boardxmax/2 -1: -1));
	board[head] = 0;

	
	draw_bg(buf, RGB(31, 63, 31));
	addNode(head, length + 1, board, buf);
	blit(buf);

	//block until the user presses one of the arrow keys, exit otherwise
	read(0, &ikey, 3);

	//Skip to the switch statement inside the do-while loop below
	if(ikey[0]== '\033' && ikey[1] == '[')
	{
		if(ikey[2]== 'A')		{move = 1; } // Up arrow 
		else if(ikey[2] == 'B') {move = 2; } // Down arrow 
		else if(ikey[2] == 'C') {move = 3; } // Right arrow 
		else if(ikey[2] == 'D') {move = 4; } // Left arrow 
	}
	else 
		move = 0; //exit the game if the user doesn't press an arrow key

	while(move > 0)
	{
		blit(buf);
		sleep_ms(400);
		key = getkey();

		if (key == 'q')
		{
			move = 0;
		}	
		else if (key == '\033' && getkey() == '[') 
		{ // if the first value is esc
			key = getkey();
			if(key== 'A')		{move = 1; } // Up arrow 
			else if(key == 'B') {move = 2; } // Down arrow 
			else if(key == 'C') {move = 3; } // Right arrow 
			else if(key == 'D') {move = 4; } // Left arrow 
    	}
	
		key = '\0';

		if(move == 1)		// Up arrow 	
		{
			head -= boardxmax; // Snake's head moves up one row.
			head = head < 0 ? (head + maxIndex): head; // Verify legal address
			i = 1;
		} 
		else if(move == 2) // Down arrow 
		{	
			head += boardxmax; // Snake's head moves up one row.
			head = (head < maxIndex) ? head: (head - maxIndex); // Verify legal address
			i = 2;
		} 
		else if(move == 3) // Right arrow 
		{	// Snake's head moves to the right one cell
			head ++; 
			head = head % boardxmax == 0 ? head - boardxmax: head;  
			i = 3;
		}
		else if(move == 4) // Left arrow 
		{	// Snake's head moves to the left one cell
			head--;
			head = (head < 0) || (head % boardxmax == boardxmax-1)? (head + boardxmax):head;
			//head = (head < 0) || (head % boardxmax == 1) ? (head + boardxmax): head;
			i = 4;
		} 

		if (head < 0 || head >= maxIndex)
			break;

		if(board[head]==0) //End of the game
		{
			draw_bg(buf, RGB(31, 0, 0));
			blit(buf);
			sleep_ms(200);
			draw_bg(buf, RGB(0, 63, 0));
			blit(buf);
			sleep_ms(200);
			i = 0;
			move = 0;
		}	
		else if (board[head] == 2) // Snake ate some food, only head moves
		{
			addNode(head, length + 1, board, buf);
		}	
		else // No food, head & tail moves
			addNode(head, length, board, buf);
	}

	

	exit_graphics();

	if (i !=0)
		printf("Problem with #%d's calculation\n", i);

	write(1, "Game Over\n", 10);
	if (maxIndex == length) puts("Congratulations, you had a perfect score!");
	else	printf("You scored %d\n", length-1);

	//Free any remaining nodes
	tempN = firstNode;
	while(firstNode)
	{	
		tempN = firstNode;
		firstNode = (Node *) (firstNode -> next);
		free(tempN);
	}

	return 0;
}


// Draws a box/sqare on the screen at a given left coordinate
static void draw_box(void *img, int x, int y, int size, color_t c)
{	
	static int i;
	for (i = 0; i < size; ++i)
		draw_line(img, x, y + i, x+size-1, y + i, c);
		// draw_line(img, x, y + i, x+size, y + i, c); Fixed segmentation fault
}


// Fills in the background with a solid color
static void draw_bg(void *img, color_t c)
{	
	static int i;
	for (i = 0; i < 480; ++i)
		draw_line(img, 0, i, 639, i, c);
}

// Puts food on the board for the snake to eat
static void food(void * img, int size, int cols, int rows, char * board)
{
	if(length<maxIndex) // Added check to make sure there were free boxes
	{
		static int x; x = rand()%cols;
		static int y; y= rand()%rows;
		static char boolean;
		int t = x + cols*y;

		boolean = 1; 
		while(boolean)
		{
			x = rand()%cols; 
			y= rand()%rows; 
			t = x + cols*y;
			if (t < maxIndex && t > -1 && board[t] == 1)
				boolean = 0;
		}

		board[t] = 2;

		draw_box(img, x*size, y*size, size, RGB(31, 0, 0));
	}
}


// Used to add new nodes & keep track of setting the colors 
static void addNode(int data, int newLen, char * board, void * img)
{
	//First node will serve as the snake's tail. The last node will be its head & where new
	//nodes are added to the linked list. 

	if (firstNode == NULL)
	{
		firstNode = malloc(sizeof (Node)); //Create a new node & link to end of the list
		firstNode -> index = data;
		firstNode -> next = NULL;
		lastNode = firstNode;
		length = 1; board[data] = 0;
		food(img, SIZE, boardxmax, boardymax, board);
	}
	else
	{		
		//Create a new node & link to end of the list
		lastNode -> next = malloc(sizeof (Node)); 
		lastNode = (Node *)(lastNode -> next); 	//Move pointer to the new node
		lastNode -> index = data;				//Set the data 
		lastNode -> next = NULL;				//Set .next of the current node to null
		board[data] = 0;

		// If the snake ate the food, put more on the board
		if (newLen > length)
		{
			length++; 
			food(img, SIZE, boardxmax, boardymax, board);
		} 
		else // Mark tail (head node) as free & change the color
		{
			int data2 = firstNode -> index;
			board[data2] = 1; 
			tempN = firstNode;
			firstNode = (Node *)(firstNode -> next);

			free(tempN);
			// End of snake disappears
			draw_box(img, (data2%boardxmax)*SIZE, 
				(data2/boardxmax)*SIZE, SIZE, RGB(31, 63, 31));
		}
	}
	// Color in new box green
	draw_box(img, (data%boardxmax)*SIZE, (data/boardxmax)*SIZE, SIZE, RGB(0, 63, 0));
}
