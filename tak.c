#include <emscripten.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

typedef enum { FLAT = 0, STANDING = 1, CAP = 2 } Piece;

typedef struct {
    uint64_t stack;
    int count;
    Piece top;
} BotTile;

typedef struct {
    int stones;
    int capstones;
} BotReserve;

extern int getBoardSize(void);
extern int returnTileCount(int tile_id);
extern uint64_t returnTileStack(int tile_id);
extern int returnTopPiece(int tile_id);
extern int getPiecesLeft(int player, int capstones);

#define MAX_TILES 64
#define MAX_MOVES 256
#define SEARCH_DEPTH 3
#define INF 100000

typedef struct {
    int tile_id;
    int piece_type;
} BotMove;

static uint64_t botSetBit(uint64_t bm, int pos, int val) {
    return (bm & ~((uint64_t)1 << pos)) | ((uint64_t)val << pos);
}

static void botPlace(BotTile board[], BotReserve res[], int tile_id, int player, int type) {
    BotTile *t = &board[tile_id];
    t->stack = botSetBit(t->stack, t->count, player);
    t->count++;
    t->top = (Piece)type;
    if (type == CAP) res[player].capstones--;
    else res[player].stones--;
}

static void botUnplace(BotTile board[], BotReserve res[], int tile_id, int player, int type) {
    BotTile *t = &board[tile_id];
    t->count--;
    t->stack = botSetBit(t->stack, t->count, 0);
    t->top = FLAT;
    if (type == CAP) res[player].capstones++;
    else res[player].stones++;
}

static int genMoves(BotTile board[], BotReserve res[], int size, int player, BotMove moves[]) {
    int n = 0;
    int total = size * size;
    for (int i = 0; i < total && n < MAX_MOVES; i++) {
        if (board[i].count > 0) continue;
        if (res[player].stones > 0) {
            moves[n++] = (BotMove){ i, FLAT };
            moves[n++] = (BotMove){ i, STANDING };
        }
        if (res[player].capstones > 0 && n < MAX_MOVES) {
            moves[n++] = (BotMove){ i, CAP };
        }
    }
    // Stack moves: TODO — requires extended BotMove with direction + drop array
    return n;
}

static int evalBoard(BotTile board[], int size, int player) {
    int score = 0;
    int total = size * size;
    for (int i = 0; i < total; i++) {
        if (board[i].count == 0) continue;
        int top_color = (int)((board[i].stack >> (board[i].count - 1)) & 1);
        if (board[i].top == FLAT) {
            score += (top_color == player) ? 2 : -2;
        }
        if (board[i].top == CAP) {
            score += (top_color == player) ? 1 : -1;
        }
    }
    return score;
}

static int minimax(BotTile board[], BotReserve res[], int size,
                   int depth, int alpha, int beta,
                   int root_player, int maximizing) {
    if (depth == 0)
        return evalBoard(board, size, root_player);

    int cur = maximizing ? root_player : (1 - root_player);
    BotMove moves[MAX_MOVES];
    int n = genMoves(board, res, size, cur, moves);

    if (n == 0)
        return maximizing ? -INF : INF;

    if (maximizing) {
        int best = -INF;
        for (int i = 0; i < n; i++) {
            botPlace(board, res, moves[i].tile_id, cur, moves[i].piece_type);
            int score = minimax(board, res, size, depth - 1, alpha, beta, root_player, 0);
            botUnplace(board, res, moves[i].tile_id, cur, moves[i].piece_type);
            if (score > best)  best = score;
            if (score > alpha) alpha = score;
            if (beta <= alpha) break;
        }
        return best;
    } else {
        int best = INF;
        for (int i = 0; i < n; i++) {
            botPlace(board, res, moves[i].tile_id, cur, moves[i].piece_type);
            int score = minimax(board, res, size, depth - 1, alpha, beta, root_player, 1);
            botUnplace(board, res, moves[i].tile_id, cur, moves[i].piece_type);
            if (score < best)  best = score;
            if (score < beta)  beta = score;
            if (beta <= alpha) break;
        }
        return best;
    }
}

// Returns tile_id * 3 + piece_type, or -1 if no move found.
EMSCRIPTEN_KEEPALIVE
int getBotMove(int player) {
    int size  = getBoardSize();
    int total = size * size;

    // Snapshot current board into local arrays
    BotTile  board[MAX_TILES];
    BotReserve res[2];
    for (int i = 0; i < total; i++) {
        board[i].count = returnTileCount(i);
        board[i].stack = returnTileStack(i);
        board[i].top   = (Piece)returnTopPiece(i);
    }
    res[0].stones    = getPiecesLeft(0, 0);
    res[0].capstones = getPiecesLeft(0, 1);
    res[1].stones    = getPiecesLeft(1, 0);
    res[1].capstones = getPiecesLeft(1, 1);

    BotMove moves[MAX_MOVES];
    int n = genMoves(board, res, size, player, moves);
    if (n == 0) return -1;

    int best_score = -INF;
    int best_move  = -1;

    for (int i = 0; i < n; i++) {
        // Fresh copy per top-level move so siblings don't see each other's changes
        BotTile  b[MAX_TILES];
        BotReserve r[2];
        memcpy(b, board, total * sizeof(BotTile));
        memcpy(r, res,   sizeof(res));

        botPlace(b, r, moves[i].tile_id, player, moves[i].piece_type);
        int score = minimax(b, r, size, SEARCH_DEPTH - 1, -INF, INF, player, 0);

        if (score > best_score) {
            best_score = score;
            best_move  = moves[i].tile_id * 3 + moves[i].piece_type;
        }
    }

    return best_move;
}
