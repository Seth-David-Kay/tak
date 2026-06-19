#include <emscripten.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>

#define BOARD_SIZE 3

typedef struct {
	uint64_t stack;
	int count;
} Tile;

typedef struct {
	// Max bitmap storage in uint64 is 64, change this storage if I increase that
	Tile* board;
} Board;

Board board = {};

EMSCRIPTEN_KEEPALIVE
void init(){
	board.board = (Tile*)calloc(BOARD_SIZE * BOARD_SIZE, sizeof(Tile));
	for (int i = 0; i < BOARD_SIZE * BOARD_SIZE; i++){
		board.board[i].stack = UINT64_MAX;
	}
}

EMSCRIPTEN_KEEPALIVE
int getBoardSize() { return BOARD_SIZE; }

void print_bitmap(uint64_t val) {
    for (int i = 63; i >= 0; i--) {
        putchar((val >> i) & 1 ? '1' : '0');
        // Optional: space every 8 bits for readability
        if (i % 8 == 0 && i != 0)
            putchar(' ');
    }
    putchar('\n');
}

// return a bitmap corresponding to the stack
// uint64 bitmap means the max stack height is 64, not a problem until 8x8 boards and up
EMSCRIPTEN_KEEPALIVE
uint64_t updateBoard(int tile) {
	uint64_t stack = board.board[tile].stack;
	int count = board.board[tile].count;

	stack = stack ^ ((1ULL<<count));
	count = count + 1;

	board.board[tile].stack = stack;
	board.board[tile].count = count;
	return 1;
}
