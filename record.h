#include <linux/spinlock.h>
#include <linux/types.h>

#define N_BOARDS 15 /* must be power of 2 minus 1*/

/**
 * record_init - Initialize the start and end of the circular queue
 */
void record_init(void);

/**
 * record_board_init - Create a new board record
 */
void record_board_init(void);

/**
 * record_board_update - Append a new move
 */
void record_board_update(int move);

/**
 * record_append_board - Append the whole board to board_record
 */
void record_append_board(void);

/**
 * record_get_board - Get the board of the specific index
 * @index: the index of the board
 *
 * User have to check if the index is valid itself
 *
 * Return: the indexth board in the queue
 */
uint64_t record_get_board(unsigned int index);


/**
 * record_get_size - Get the size of the circular queue
 *
 * Return: the number of board recorded
 */
int record_get_size(void);