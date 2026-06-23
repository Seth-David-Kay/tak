# commands: make, make run, make clean

EMSDK_DIR = deps/emsdk
EMCC = $(EMSDK_DIR)/upstream/emscripten/emcc

SRC = main.c
OUT = main.js

all: $(OUT)

$(OUT): $(SRC)
	@source $(EMSDK_DIR)/emsdk_env.sh > /dev/null 2>&1 && \
	$(EMCC) $(SRC) -o $(OUT) \
	-s MODULARIZE=1 \
	-s EXPORT_ES6=1 \
	-s EXPORTED_FUNCTIONS='["_getBoardSize", "_init", "_updateBoardLeftClick", "_updateBoardRightClick", "_getTileStack", "_return_tile_count", "_return_tile_stack", "_return_top_piece", "_placeStone"]' \
	-s EXPORTED_RUNTIME_METHODS='["cwrap"]'

run: all
	@echo "Starting server at http://localhost:8001/tak.html"
	@python -m http.server 8001

clean:
	rm -f main.js main.wasm
