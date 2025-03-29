#include "record.h"

static uint64_t board_record[N_BOARDS + 1];
static uint64_t now_board;
static uint8_t now_idx;
static uint8_t start, end;

void record_init(void)
{
    start = end = 0;
    pr_info("record_init: init\n");
}

void record_board_init(void)
{
    now_board = 0;
    now_idx = 0;
    pr_info("record_board_init: board init\n");
}

void record_board_update(int move)
{
    now_board |= ((uint64_t) move) << now_idx;
    now_idx += 4;
}


void record_append_board(void)
{
    if (((start - end) & N_BOARDS) == 1)
        start = (start + 1) & N_BOARDS;

    board_record[end] = now_board;
    if (now_idx != 64) {
        now_idx >>= 2;
        now_board ^= (now_board >> 4);
        now_board ^= (now_board >> 8);
        now_board ^= (now_board >> 16);
        now_board ^= (now_board >> 32);
        now_board ^= now_idx;
        board_record[end] |= ((now_board & 0xf) << 60);
    }
    end = (end + 1) & N_BOARDS;
    pr_info("record_append_board: %d %llx\n", end, now_board);
}

uint64_t record_get_board(unsigned int index)
{
    return board_record[(start + index) & N_BOARDS];
}

int record_get_size(void)
{
    return (end - start) & N_BOARDS;
}