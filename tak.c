/*
 * Tak bot (Emscripten / WASM)
 * -----------------------------------------------------------------
 * Slide model: a "slide" (stack move) carries `count` pieces and
 * steps onto `squares` squares in a straight line (squares <= count).
 * Every square except the last gets exactly 1 piece; the LAST square
 * absorbs whatever remains (count - squares + 1 pieces). This covers
 * the vast majority of real Tak slides, including the single most
 * important pattern the previous version could not represent at all:
 * carrying several pieces and dropping more than one of them onto the
 * final square (e.g. "pick up 3, move 1 square, drop all 3" to finish
 * a road). The old version forced squares == count (1 piece per
 * square, always), which made it blind to that whole class of moves
 * -- including when the OPPONENT had one available, since the search
 * generates the opponent's candidate moves with the same generator
 * it uses for itself. That's why stack wins were being missed.
 *
 * (The one case still not modeled is an uneven split across more than
 * one square, e.g. drop 2 then 1 then 1. That pattern is rare and
 * tactically far less important than the "dump the remainder on the
 * landing square" pattern above, so it's left out to keep the search
 * fast.)
 *
 * Output contract: [click_count, source_tile, step_tile_1, step_tile_2,
 * ..., -1]. Each entry after the source is one "piece dropped here"
 * click; if the final square receives more than one piece, its tile
 * id simply repeats that many times in a row before the -1 marker.
 * This is a strict extension of the original format -- single-drop
 * moves look exactly as before.
 *
 * Fixed / added vs. the original:
 *   - Slides can now drop a multi-piece stack onto their landing
 *     square instead of being forced to spread 1 piece per square.
 *   - Correct blocking rules: standing stones and capstones block a
 *     slide, EXCEPT a lone capstone may flatten a standing stone by
 *     landing on it alone as the final square of its move.
 *   - Road-win detection (BFS flood fill) used as a hard terminal
 *     condition inside the search, not just a flat-count heuristic.
 *   - Game-over-by-board-full / game-over-by-empty-reserves handled
 *     via flat-count comparison instead of being silently ignored.
 *   - First-two-plies "swap rule" (you place a flat of your
 *     opponent's color) is respected during search.
 *   - Evaluation function now considers road-completion distance
 *     (via Dijkstra), not just raw piece counts.
 */

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

#define MAX_TILES     64
#define MAX_MOVES     600
#define WIN_SCORE     1000000
#define INF           2000000000
#define MIN_DEPTH     1
#define MAX_DEPTH     6   /* raise with care -- cost grows roughly exponentially */

/* Difficulty / search-time knob. Larger = stronger but slower.
   Change it at runtime via setSearchDepth(); defaults to 3 (the
   original hardcoded value). */
static int g_search_depth = 3;

EMSCRIPTEN_KEEPALIVE
void setSearchDepth(int depth) {
    if (depth < MIN_DEPTH) depth = MIN_DEPTH;
    if (depth > MAX_DEPTH) depth = MAX_DEPTH;
    g_search_depth = depth;
}

EMSCRIPTEN_KEEPALIVE
int getSearchDepth(void) {
    return g_search_depth;
}

typedef struct {
    int is_move;     /* 0 = Placement, 1 = Stack Move (Slide) */
    int tile_id;     /* Target tile for placement, source tile for slide */
    int piece_type;  /* For placement: FLAT, STANDING, CAP */
    int dir;         /* For slide: 0=Up, 1=Down, 2=Left, 3=Right */
    int count;       /* For slide: total number of pieces carried */
    int squares;     /* For slide: number of squares stepped onto (<= count).
                         Squares 1..squares-1 each get 1 piece; square
                         `squares` (the landing square) gets whatever is
                         left: count - (squares - 1) pieces. */
} BotMove;

/* Static buffer used to pass the sequential click instructions back to JavaScript */
static int js_move_output[32];

/* ---------------------------------------------------------------- */
/* Bit / tile helpers                                                */
/* ---------------------------------------------------------------- */

static uint64_t botSetBit(uint64_t bm, int pos, int val) {
    return (bm & ~((uint64_t)1 << pos)) | ((uint64_t)val << pos);
}

static int tileOwner(const BotTile *t) {
    return (int)((t->stack >> (t->count - 1)) & 1ULL);
}

/* A cell counts toward a road if it holds a flat or capstone top owned by player */
static int isRoadPiece(const BotTile *t, int player) {
    return t->count > 0 && t->top != STANDING && tileOwner(t) == player;
}

/* First two plies of the game: you must place a single flat of your
   opponent's color. Detected by total piece count on board. */
static int isSwapPly(const BotTile board[], int size) {
    int total = size * size;
    int cnt = 0;
    for (int i = 0; i < total; i++) cnt += board[i].count;
    return cnt < 2;
}

/* ---------------------------------------------------------------- */
/* Move application                                                  */
/* ---------------------------------------------------------------- */

static void botPlace(BotTile board[], BotReserve res[], int tile_id, int owner, int type) {
    BotTile *t = &board[tile_id];
    t->stack = botSetBit(t->stack, t->count, owner);
    t->count++;
    t->top = (Piece)type;
    if (type == CAP) res[owner].capstones--;
    else res[owner].stones--;
}

static void botApplyMove(BotTile board[], BotReserve res[], int size, int player, const BotMove *m) {
    if (!m->is_move) {
        int owner = isSwapPly(board, size) ? (1 - player) : player;
        botPlace(board, res, m->tile_id, owner, m->piece_type);
        return;
    }

    int dirs[4] = { size, -size, -1, 1 }; /* Up, Down, Left, Right index offsets */
    int offset = dirs[m->dir];
    BotTile *src = &board[m->tile_id];

    /* Extract moving stack ownership bits from the top of the stack */
    uint64_t moving_stack = 0;
    for (int i = 0; i < m->count; i++) {
        int bit_pos = src->count - m->count + i;
        int p_owner = (src->stack >> bit_pos) & 1;
        moving_stack |= ((uint64_t)p_owner << i);
    }
    Piece moving_top_type = src->top;

    /* Truncate the source stack. Anything left underneath must be flat,
       since only the very top of a stack can be a wall or capstone. */
    src->count -= m->count;
    if (src->count > 0) {
        src->stack &= ((uint64_t)1 << src->count) - 1;
        src->top = FLAT;
    } else {
        src->stack = 0;
        src->top = FLAT;
    }

    /* Drop pieces along the path: 1 piece on each square before the last,
       then whatever remains on the landing square. */
    int curr_tile = m->tile_id;
    int piece_idx = 0; /* index into moving_stack, bottom of the carried group first */
    for (int sq = 1; sq <= m->squares; sq++) {
        curr_tile += offset;
        BotTile *dst = &board[curr_tile];
        int dropCount = (sq < m->squares) ? 1 : (m->count - (m->squares - 1));

        for (int k = 0; k < dropCount; k++) {
            int p_owner = (moving_stack >> piece_idx) & 1;
            dst->stack = botSetBit(dst->stack, dst->count, p_owner);
            dst->count++;
            piece_idx++;
        }
        /* Only the landing square receives the true top piece type (which
           may flatten a standing stone underneath it); every other square,
           and every non-final piece dropped on the landing square, is a
           flat, matching genMoves' legality checks below. */
        dst->top = (sq == m->squares) ? moving_top_type : FLAT;
    }
}

/* ---------------------------------------------------------------- */
/* Move generation                                                   */
/* ---------------------------------------------------------------- */

static int genMoves(BotTile board[], BotReserve res[], int size, int player, BotMove moves[]) {
    int n = 0;
    int total = size * size;
    int swap = isSwapPly(board, size);
    int placeOwner = swap ? (1 - player) : player;

    /* 1. Generate Placements */
    for (int i = 0; i < total && n < MAX_MOVES; i++) {
        if (board[i].count > 0) continue;

        if (swap) {
            /* Swap rule: single flat only, drawn from the opponent's reserve */
            if (res[placeOwner].stones > 0) {
                moves[n++] = (BotMove){ 0, i, FLAT, 0, 0 };
            }
            continue;
        }

        if (res[player].stones > 0) {
            moves[n++] = (BotMove){ 0, i, FLAT, 0, 0 };
            if (n < MAX_MOVES) moves[n++] = (BotMove){ 0, i, STANDING, 0, 0 };
        }
        if (res[player].capstones > 0 && n < MAX_MOVES) {
            moves[n++] = (BotMove){ 0, i, CAP, 0, 0 };
        }
    }

    /* No stack moves are legal on the first two plies of the game */
    if (swap) return n;

    /* 2. Generate Stack Moves (Slides) */
    int dirs[4][2] = { {0,1}, {0,-1}, {-1,0}, {1,0} }; /* Up, Down, Left, Right */
    for (int i = 0; i < total && n < MAX_MOVES; i++) {
        if (board[i].count == 0) continue;
        if (tileOwner(&board[i]) != player) continue; /* must control the top of the stack */

        int max_carry = board[i].count > size ? size : board[i].count;
        Piece src_top = board[i].top;
        int x = i % size;
        int y = i / size;

        for (int d = 0; d < 4 && n < MAX_MOVES; d++) {
            /* Walk the direction once to find:
                 - flat_run: how many consecutive squares from the start are
                   empty/FLAT (i.e. can be stepped onto, and can be the
                   landing square with any number of pieces on them).
                 - flatten_step: the square right after that run, IF it's a
                   standing stone AND the moving stack's top is a capstone
                   (so it can be flattened -- but only as a lone,
                   single-piece landing). -1 if not applicable. */
            int flat_run = 0;
            int flatten_step = -1;
            int nx = x, ny = y;
            for (int step = 1; step <= max_carry; step++) {
                nx += dirs[d][0];
                ny += dirs[d][1];
                if (nx < 0 || nx >= size || ny < 0 || ny >= size) break;

                int tid = ny * size + nx;
                BotTile *tgt = &board[tid];

                if (tgt->count == 0 || tgt->top == FLAT) { flat_run = step; continue; }
                if (tgt->top == STANDING && src_top == CAP) { flatten_step = step; }
                break; /* CAP always blocks; STANDING blocks unless just flagged above */
            }

            if (flat_run == 0 && flatten_step == -1) continue; /* nowhere to go this direction */

            for (int count = 1; count <= max_carry && n < MAX_MOVES; count++) {
                /* Landing on any square within the flat run: squares 1..s-1 get
                   1 piece each, square s (<= flat_run) absorbs the remainder. */
                int maxSquares = flat_run < count ? flat_run : count;
                for (int s = 1; s <= maxSquares && n < MAX_MOVES; s++) {
                    moves[n++] = (BotMove){ 1, i, 0, d, count, s };
                }
                /* Flattening landing: only legal when the whole carried group
                   is exactly `flatten_step` pieces, so the landing square
                   receives exactly 1 (the capstone), as required to flatten. */
                if (flatten_step != -1 && count == flatten_step && n < MAX_MOVES) {
                    moves[n++] = (BotMove){ 1, i, 0, d, count, flatten_step };
                }
            }
        }
    }
    return n;
}

/* ---------------------------------------------------------------- */
/* Road detection                                                    */
/* ---------------------------------------------------------------- */

static int hasRoad(BotTile board[], int size, int player) {
    int total = size * size;
    int visited[MAX_TILES];
    int queue[MAX_TILES];
    int dirs[4][2] = { {0,1}, {0,-1}, {-1,0}, {1,0} };

    /* Horizontal: left column -> right column */
    memset(visited, 0, sizeof(int) * total);
    int qh = 0, qt = 0;
    for (int y = 0; y < size; y++) {
        int id = y * size + 0;
        if (isRoadPiece(&board[id], player)) { visited[id] = 1; queue[qt++] = id; }
    }
    while (qh < qt) {
        int id = queue[qh++];
        int x = id % size, y = id / size;
        if (x == size - 1) return 1;
        for (int d = 0; d < 4; d++) {
            int nx = x + dirs[d][0], ny = y + dirs[d][1];
            if (nx < 0 || nx >= size || ny < 0 || ny >= size) continue;
            int nid = ny * size + nx;
            if (visited[nid] || !isRoadPiece(&board[nid], player)) continue;
            visited[nid] = 1; queue[qt++] = nid;
        }
    }

    /* Vertical: top row -> bottom row */
    memset(visited, 0, sizeof(int) * total);
    qh = 0; qt = 0;
    for (int x = 0; x < size; x++) {
        int id = 0 * size + x;
        if (isRoadPiece(&board[id], player)) { visited[id] = 1; queue[qt++] = id; }
    }
    while (qh < qt) {
        int id = queue[qh++];
        int x = id % size, y = id / size;
        if (y == size - 1) return 1;
        for (int d = 0; d < 4; d++) {
            int nx = x + dirs[d][0], ny = y + dirs[d][1];
            if (nx < 0 || nx >= size || ny < 0 || ny >= size) continue;
            int nid = ny * size + nx;
            if (visited[nid] || !isRoadPiece(&board[nid], player)) continue;
            visited[nid] = 1; queue[qt++] = nid;
        }
    }

    return 0;
}

/* ---------------------------------------------------------------- */
/* Evaluation                                                         */
/* ---------------------------------------------------------------- */

static int flatScore(BotTile board[], int size, int player) {
    int total = size * size;
    int score = 0;
    for (int i = 0; i < total; i++) {
        if (board[i].count == 0 || board[i].top == STANDING) continue;
        score += (tileOwner(&board[i]) == player) ? 1 : -1;
    }
    return score;
}

/* Cost (in "pieces still needed") to route a road through this cell */
static int cellCost(const BotTile *t, int player) {
    if (t->count == 0) return 1;                 /* empty: needs a placement */
    if (t->top == STANDING) return INF;           /* walls block road-building */
    return (tileOwner(t) == player) ? 0 : INF;    /* own flat/cap free, opponent's blocks */
}

/* Cheap Dijkstra (board is tiny, O(n^2) is trivial) from one edge to the
   opposite edge. horizontal=1 -> left to right, horizontal=0 -> top to bottom. */
static int roadCost(BotTile board[], int size, int player, int horizontal) {
    int total = size * size;
    int dist[MAX_TILES];
    int done[MAX_TILES];
    for (int i = 0; i < total; i++) { dist[i] = INF; done[i] = 0; }

    for (int k = 0; k < size; k++) {
        int id = horizontal ? (k * size + 0) : (0 * size + k);
        int c = cellCost(&board[id], player);
        if (c < dist[id]) dist[id] = c;
    }

    int dirs[4][2] = { {0,1}, {0,-1}, {-1,0}, {1,0} };
    for (int iter = 0; iter < total; iter++) {
        int u = -1, best = INF;
        for (int i = 0; i < total; i++) if (!done[i] && dist[i] < best) { best = dist[i]; u = i; }
        if (u == -1) break;
        done[u] = 1;
        int x = u % size, y = u / size;
        for (int d = 0; d < 4; d++) {
            int nx = x + dirs[d][0], ny = y + dirs[d][1];
            if (nx < 0 || nx >= size || ny < 0 || ny >= size) continue;
            int v = ny * size + nx;
            if (done[v]) continue;
            int c = cellCost(&board[v], player);
            if (c >= INF) continue;
            if (dist[u] + c < dist[v]) dist[v] = dist[u] + c;
        }
    }

    int best = INF;
    for (int k = 0; k < size; k++) {
        int id = horizontal ? (k * size + (size - 1)) : ((size - 1) * size + k);
        if (dist[id] < best) best = dist[id];
    }
    return best;
}

static int roadDist(BotTile board[], int size, int player) {
    int h = roadCost(board, size, player, 1);
    int v = roadCost(board, size, player, 0);
    return h < v ? h : v;
}

static int evalBoard(BotTile board[], int size, int player) {
    int opp = 1 - player;
    int flatDiff = 0, wallDiff = 0, capDiff = 0;
    int total = size * size;

    for (int i = 0; i < total; i++) {
        if (board[i].count == 0) continue;
        int mine = (tileOwner(&board[i]) == player);
        switch (board[i].top) {
            case FLAT:     flatDiff += mine ? 1 : -1; break;
            case STANDING: wallDiff += mine ? 1 : -1; break;
            case CAP:      capDiff  += mine ? 1 : -1; break;
        }
    }

    int cap = size * 3; /* clamp unreachable (INF) road distances */
    int myR  = roadDist(board, size, player);
    int oppR = roadDist(board, size, opp);
    if (myR  > cap) myR  = cap;
    if (oppR > cap) oppR = cap;

    int roadScore = (oppR - myR) * 4;

    return flatDiff * 3 + wallDiff * 1 + capDiff * 2 + roadScore;
}

/* ---------------------------------------------------------------- */
/* Terminal-state check (road win, board full, reserves empty)       */
/* ---------------------------------------------------------------- */

static int checkTerminal(BotTile b[], BotReserve r[], int size, int root_player, int mover, int *out_score) {
    if (hasRoad(b, size, mover)) {
        *out_score = (mover == root_player) ? WIN_SCORE : -WIN_SCORE;
        return 1;
    }

    int total = size * size, filled = 0;
    for (int i = 0; i < total; i++) if (b[i].count > 0) filled++;

    int p0_out = (r[0].stones + r[0].capstones == 0);
    int p1_out = (r[1].stones + r[1].capstones == 0);

    if (filled == total || p0_out || p1_out) {
        int fs = flatScore(b, size, root_player);
        *out_score = (fs > 0) ? (WIN_SCORE / 2) : ((fs < 0) ? -(WIN_SCORE / 2) : 0);
        return 1;
    }

    return 0;
}

/* ---------------------------------------------------------------- */
/* Search                                                             */
/* ---------------------------------------------------------------- */

static int minimax(BotTile board[], BotReserve res[], int size,
                    int depth, int alpha, int beta,
                    int root_player, int maximizing) {
    if (depth == 0)
        return evalBoard(board, size, root_player);

    int cur = maximizing ? root_player : (1 - root_player);
    BotMove moves[MAX_MOVES];
    int n = genMoves(board, res, size, cur, moves);

    if (n == 0) {
        int fs = flatScore(board, size, root_player);
        return (fs > 0) ? (WIN_SCORE / 2) : ((fs < 0) ? -(WIN_SCORE / 2) : 0);
    }

    if (maximizing) {
        int best = -INF;
        for (int i = 0; i < n; i++) {
            BotTile b[MAX_TILES]; BotReserve r[2];
            memcpy(b, board, size * size * sizeof(BotTile));
            memcpy(r, res, sizeof(r));
            botApplyMove(b, r, size, cur, &moves[i]);

            int score;
            if (!checkTerminal(b, r, size, root_player, cur, &score))
                score = minimax(b, r, size, depth - 1, alpha, beta, root_player, 0);

            if (score > best)  best = score;
            if (score > alpha) alpha = score;
            if (beta <= alpha) break;
        }
        return best;
    } else {
        int best = INF;
        for (int i = 0; i < n; i++) {
            BotTile b[MAX_TILES]; BotReserve r[2];
            memcpy(b, board, size * size * sizeof(BotTile));
            memcpy(r, res, sizeof(r));
            botApplyMove(b, r, size, cur, &moves[i]);

            int score;
            if (!checkTerminal(b, r, size, root_player, cur, &score))
                score = minimax(b, r, size, depth - 1, alpha, beta, root_player, 1);

            if (score < best) best = score;
            if (score < beta) beta = score;
            if (beta <= alpha) break;
        }
        return best;
    }
}

EMSCRIPTEN_KEEPALIVE
int* getBotMove(int player) {
    int size  = getBoardSize();
    int total = size * size;
    if (size <= 0 || total > MAX_TILES) return NULL;

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
    if (n == 0) return NULL;

    int best_score = -INF;
    int best_idx   = -1;

    for (int i = 0; i < n; i++) {
        BotTile b[MAX_TILES]; BotReserve r[2];
        memcpy(b, board, total * sizeof(BotTile));
        memcpy(r, res, sizeof(res));
        botApplyMove(b, r, size, player, &moves[i]);

        int score;
        if (!checkTerminal(b, r, size, player, player, &score))
            score = minimax(b, r, size, g_search_depth - 1, -INF, INF, player, 0);

        if (score > best_score) {
            best_score = score;
            best_idx   = i;
        }
        if (best_score >= WIN_SCORE) break; /* forced win found, stop searching */
    }

    if (best_idx == -1) return NULL;

    BotMove m = moves[best_idx];
    if (!m.is_move) {
        /* [Click Count, Tile ID, Piece Type (FLAT=0, STANDING=1, CAP=2)] */
        js_move_output[0] = 1;
        js_move_output[1] = m.tile_id;
        js_move_output[2] = m.piece_type;
    } else {
        /* [Click Count, Source Tile ID, Step 1 Tile ID, Step 2 Tile ID..., -1 Marker]
           One entry per piece dropped. If the landing square receives more
           than one piece, its tile id simply appears that many times in a
           row right before the -1 marker. */
        int dirs[4][2] = { {0,1}, {0,-1}, {-1,0}, {1,0} };
        int x = m.tile_id % size;
        int y = m.tile_id / size;

        js_move_output[0] = 1 + m.count;
        js_move_output[1] = m.tile_id;

        int idx = 2;
        int nx = x, ny = y;
        for (int sq = 1; sq <= m.squares; sq++) {
            nx += dirs[m.dir][0];
            ny += dirs[m.dir][1];
            int tid = ny * size + nx;
            int dropCount = (sq < m.squares) ? 1 : (m.count - (m.squares - 1));
            for (int k = 0; k < dropCount; k++) js_move_output[idx++] = tid;
        }
        js_move_output[idx] = -1; /* Denotes a slide movement */
    }

    return js_move_output;
}
