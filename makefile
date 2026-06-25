# commands: make, make run, make clean

EMSDK_DIR = deps/emsdk
EMCC = $(EMSDK_DIR)/upstream/emscripten/emcc

SRC = main.c tak.c
OUT = main.js

all: $(OUT)

$(OUT): $(SRC)
	@source $(EMSDK_DIR)/emsdk_env.sh > /dev/null 2>&1 && \
	$(EMCC) $(SRC) -o $(OUT) \
	-s MODULARIZE=1 \
	-s EXPORT_ES6=1 \
	-s EXPORTED_FUNCTIONS='["_getBoardSize", "_init", "_updateBoardLeftClick", "_updateBoardRightClick", "_getTileStack", "_returnTileCount", "_returnTileStack", "_returnTopPiece", "_placeStone", "_getPiecesLeft", "_endTurn", "_reset", "_getBufferCount", "_canPlayerSelect", "_getMovingToTileId", "_getBufferStack", "_getBufferTop", "_canMoveBufferTo", "_restartGame", "_getBotMove", "_isGameOver"]' \
	-s EXPORTED_RUNTIME_METHODS='["cwrap"]'

run: all
	@echo "Starting server at http://localhost:8001/tak.html"
	@python -m http.server 8001

clean:
	rm -f main.js main.wasm
