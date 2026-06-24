#include <emscripten.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

static int BOARD_SIZE = 3;

void printBitmap(uint64_t);
uint64_t setBit(uint64_t, int, int);
uint64_t getBit(uint64_t, int);
int checkIfValidMove(int, int);
int calculateDirection(int, int);
void saveSnapshot();

typedef enum {
	FLAT = 0,
	STANDING = 1,
	CAP = 2,
} Piece;

typedef struct {
	uint64_t stack;
	int count;
	Piece top;
	int id;
} Tile;

typedef struct {
	int stones;
	int capstones;
} Reserve;

static Tile *board = NULL;
static Tile move_buffer = {};
static int move_direction = -1;
static int moving_to_tile_id = -1; // what tile is currently being placed on
static int origin_moving_tile_id = -1; // what tile tiles were taken from
static Reserve reserve[2] = {}; // reserve[0] is white
static const int STONE_COUNTS[] = {0, 0, 0, 10, 15, 21, 30, 40, 50};
static const int CAPSTONE_COUNTS[] = {0, 0, 0, 0, 0, 1, 1, 2, 2};
static Tile *board_save = NULL;
static Reserve reserve_save[2] = {};

EMSCRIPTEN_KEEPALIVE
void init() {
	board = (Tile *)calloc(BOARD_SIZE * BOARD_SIZE, sizeof(Tile));
	for (int i = 0; i < BOARD_SIZE * BOARD_SIZE; i++)
		board[i].id = i;
	reserve[0].stones = STONE_COUNTS[BOARD_SIZE];
	reserve[1].stones = STONE_COUNTS[BOARD_SIZE];
	reserve[0].capstones = CAPSTONE_COUNTS[BOARD_SIZE];
	reserve[1].capstones = CAPSTONE_COUNTS[BOARD_SIZE];
	move_buffer.id = -1;
	saveSnapshot();
}

void pop(Tile *tile) {
	// checks
	// can not pop if no pieces left on tile
	// can not pop if trying to pickup from two tiles at once, aka
	// if buffer has something in it AND buffer origin tile != tile
	if ((tile->count == 0) || (move_buffer.count != 0 && origin_moving_tile_id != tile->id))
		return;

	// set move buffer at move buffer count = tile at tile count
	int tile_stack_bit = getBit(tile->stack, tile->count - 1);
	move_buffer.stack = setBit(move_buffer.stack, move_buffer.count, tile_stack_bit);
	// if first piece in move buffer, save top information
	if (move_buffer.count == 0){
		move_buffer.top = tile->top;
	}
	tile->count -= 1;
	tile->top = FLAT;
	move_buffer.count += 1;
}
// returns 1 on success, -1 on failure
int push(Tile *tile) {
	// checks
	// can not push if buffer count is 0
	// can not push if the tile top is not flat
	// (TODO) can not push if tile is not in move direction if already part of a
	// move using move_direction
	if (move_buffer.count == 0) return -1;
	if (tile->count > 0){
		if (tile->top == STANDING && move_buffer.count == 1 && move_buffer.top == CAP){
			tile->top = FLAT;
		} else if (tile->top != FLAT) return -1;
	}
	// if move == none, fine. else, see if moving in same direction

	// Push top of buffer stack onto top of tile stack, adjust counts accordingly
	int buffer_stack_bit = getBit(move_buffer.stack, move_buffer.count - 1);
	tile->stack = setBit(tile->stack, tile->count, buffer_stack_bit);
	// placing top move buffer piece onto tile stack
	if (move_buffer.count == 1){
		tile->top = move_buffer.top;
	}
	tile->count += 1;
	move_buffer.count -= 1;
	return 1;
}

// end turn: if direction != none & buffer.count == 0 (moved already & nothing
// being held)

// reset:
// reset buffer if buffer != 0 or direction != none, reset
// reset: store copy of board, override when end turn is clicked

// Soft set, returns new value, does not edit original value
uint64_t setBit(uint64_t bitmap, int position, int set_value) {
	if (set_value != 1 && set_value != 0)
		return bitmap;
	return (bitmap & ~((uint64_t)1 << position)) |
				 ((uint64_t)set_value << position);
}

uint64_t getBit(uint64_t bitmap, int position) {
	return (bitmap >> position) & 1;
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

EMSCRIPTEN_KEEPALIVE
void placeStone(int tile_id, int color, int type){
	Tile *tile = &board[tile_id];
	if (tile->count > 0) return;
	if (type == 0 || 1){
		if (reserve[color].stones == 0) return;
	} else {
		if (reserve[color].capstones == 0) return;
	}

	tile->top = type;
	tile->stack = setBit(tile->stack, 0, color);
	tile->count += 1;

	if (type == 0 || 1){
		reserve[color].stones -= 1;
	}else {
		reserve[color].capstones -= 1;
	}
}

void saveSnapshot() {
	memcpy(board_save, board, BOARD_SIZE * BOARD_SIZE * sizeof(Tile));
	reserve_save[0] = reserve[0];
	reserve_save[1] = reserve[1];
}

EMSCRIPTEN_KEEPALIVE
void updateBoardLeftClick(int tile_id) {
	// if left click & nothing, pop
	// if left click & selected &! moved, pop if valid move (four touching selected)
	// if left click & selected & moved, pop if valid move (four touching moved)
	Tile *tile = &board[tile_id];
	if (origin_moving_tile_id < 0) {
		origin_moving_tile_id = tile_id;
		pop(tile);
	}
	else if (tile_id == origin_moving_tile_id && moving_to_tile_id < 0){
		if (move_buffer.count != BOARD_SIZE){
			pop(tile);
		}
	}
	else if (moving_to_tile_id < 0){
	 	if (checkIfValidMove(origin_moving_tile_id, tile_id)){
	 		move_direction = calculateDirection(origin_moving_tile_id, tile_id); // (TODO) calculate direction
	 		int valid = push(tile);
	 		if (valid){
	 			moving_to_tile_id = tile_id;
	 		} else {
	 			move_direction = -1;
	 		}
	 	}
	}
	else {
		if (tile_id == moving_to_tile_id) {
			push(tile);
		} else if (checkIfValidMove(moving_to_tile_id, tile_id) &&
			calculateDirection(moving_to_tile_id, tile_id) == move_direction) {
			int valid = push(tile);
			if (valid) moving_to_tile_id = tile_id;
		}
	}
}

EMSCRIPTEN_KEEPALIVE
int canMoveBufferTo(int tile_id) {
    Tile *tile = &board[tile_id];
    if (move_buffer.count == 0) return 0;
    if (tile->count == 0) return 1;
    if (tile->top == FLAT) return 1;
    // only legal exception: capstone crushing a standing stone
    if (tile->top == STANDING && move_buffer.count == 1 && move_buffer.top == CAP) return 1;
    return 0;
}

// 0 = up, 1 = down, 2 = left, 3 = right, -1 = same tile
int calculateDirection(int from_id, int to_id) {
    int from_row = from_id / BOARD_SIZE;
    int from_col = from_id % BOARD_SIZE;
    int to_row   = to_id   / BOARD_SIZE;
    int to_col   = to_id   % BOARD_SIZE;
    if (to_row < from_row) return 0;
    if (to_row > from_row) return 1;
    if (to_col < from_col) return 2;
    if (to_col > from_col) return 3;
    return -1;
}

// Returns 0 if false, 1 if true
int checkIfValidMove(int tile_id_from, int tile_id_to){
	int from_row = floor(tile_id_from / BOARD_SIZE);
	int from_col = tile_id_from % BOARD_SIZE;
	int to_row = floor(tile_id_to / BOARD_SIZE);
	int to_col = tile_id_to % BOARD_SIZE;
	if (from_row == to_row && ((from_col == (to_col + 1)) || (from_col == (to_col - 1))))
		return 1;
	if (from_col == to_col && ((from_row == (to_row + 1)) || (from_row == (to_row - 1))))
		return 1;
	return 0;
}

EMSCRIPTEN_KEEPALIVE
// to implement later if I want, right click rn is not going to do anything
// 	other than test
void updateBoardRightClick(int tile_id) {
	Tile *tile = &board[tile_id];
	// tests
	tile->stack = setBit(tile->stack, tile->count, 1);
	tile->count += 1;
	// end tests
	return;
}

EMSCRIPTEN_KEEPALIVE
int canPlayerSelect(int tile_id, int player){
	Tile *tile = &board[tile_id];
	if (tile->count == 0) return 0;
	return getBit(tile->stack, tile->count - 1) == player;
}

EMSCRIPTEN_KEEPALIVE
void reset(){
	memcpy(board, board_save, BOARD_SIZE * BOARD_SIZE * sizeof(Tile));
	reserve[0] = reserve_save[0];
	reserve[1] = reserve_save[1];
	move_buffer.stack = 0;
	move_buffer.count = 0;
	move_buffer.top = FLAT;
	move_direction = -1;
	moving_to_tile_id = -1;
	origin_moving_tile_id = -1;
}

EMSCRIPTEN_KEEPALIVE
void endTurn(){
	move_buffer.stack = 0;
	move_buffer.count = 0;
	move_buffer.top = FLAT;
	move_direction = -1;
	moving_to_tile_id = -1;
	origin_moving_tile_id = -1;
	saveSnapshot();
}

EMSCRIPTEN_KEEPALIVE
void restartGame(){
	endTurn();
}

EMSCRIPTEN_KEEPALIVE
int getMovingToTileId(){
	return moving_to_tile_id;
}

EMSCRIPTEN_KEEPALIVE
uint64_t getBufferStack(){
	return move_buffer.stack;
}

EMSCRIPTEN_KEEPALIVE
int getBufferTop(){
	return move_buffer.top;
}

EMSCRIPTEN_KEEPALIVE
int getBoardSize() { return BOARD_SIZE; }

EMSCRIPTEN_KEEPALIVE
int returnTileCount(int tile_id){
	return board[tile_id].count;
}


EMSCRIPTEN_KEEPALIVE
int returnTopPiece(int tile_id){
	return board[tile_id].top;
}

EMSCRIPTEN_KEEPALIVE
uint64_t returnTileStack(int tile_id){
	return board[tile_id].stack;
}

EMSCRIPTEN_KEEPALIVE
uint64_t getTileStack(int tile_id) { return board[tile_id].stack; }

EMSCRIPTEN_KEEPALIVE
int getBufferCount() { return move_buffer.count; }

EMSCRIPTEN_KEEPALIVE
// 0 is white, 1 is return capstone count
int getPiecesLeft(int player, int capstones){
	if (player > 1 || player < 0) return -1;
	if (capstones) {
		return reserve[player].capstones;
	}
	else {
		return reserve[player].stones;
	}
}
