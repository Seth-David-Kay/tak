#include <emscripten.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>

#define BOARD_SIZE 3

void printBitmap(uint64_t);
uint64_t setBit(uint64_t, int);

typedef enum {
	FLAT = 0,
	STANDING = 1,
	CAP = 2,
} Piece;

typedef struct {
	uint64_t stack;
	int count;
	Piece top;
} Tile;

typedef struct {
	int stones;
	int capstones;
} Reserve;

static Tile* board = NULL;
static Tile move_buffer = {};
static int move_direction = 0;
static Reserve reserve[2] = {}; // reserve[0] is white
static const int STONE_COUNTS[] = {0, 0, 0, 0, 15, 21, 30, 40, 50};
static const int CAPSTONE_COUNTS[] = {0, 0, 0, 0, 0, 1, 1, 2, 2};

EMSCRIPTEN_KEEPALIVE
void init(){
	board = (Tile*)calloc(BOARD_SIZE*BOARD_SIZE, sizeof(Tile));
	reserve[0].stones    = STONE_COUNTS[BOARD_SIZE];
    reserve[1].stones    = STONE_COUNTS[BOARD_SIZE];
    reserve[0].capstones = CAPSTONE_COUNTS[BOARD_SIZE];
    reserve[1].capstones = CAPSTONE_COUNTS[BOARD_SIZE];
    move_buffer.stack = setBit(move_buffer.stack, 1);
    printBitmap(move_buffer.stack);
}

EMSCRIPTEN_KEEPALIVE
int getBoardSize() { return BOARD_SIZE; }

uint64_t setBit(uint64_t x, int i){
    return x ^ ((1<<i));
}

void printBitmap(uint64_t val) {
    for (int i = 63; i >= 0; i--) {
        putchar((val >> i) & 1 ? '1' : '0');
        // Optional: space every 8 bits for readability
        if (i % 8 == 0 && i != 0)
            putchar(' ');
    }
    putchar('\n');
}
