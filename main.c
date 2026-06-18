#include <emscripten.h>

#define BOARD_SIZE 3

EMSCRIPTEN_KEEPALIVE
int add(int a, int b) { return a + b; }

EMSCRIPTEN_KEEPALIVE
int sub(int a, int b) { return a - b; }

EMSCRIPTEN_KEEPALIVE
int getBoardSize() { return BOARD_SIZE; }
