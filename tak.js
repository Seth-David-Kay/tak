import createModule from "./main.js";

const Module = await createModule();

Module._init();
let current_turn = 0; // 0 is white
let selectedPiece = -1; // -1 is nothing, 0 if flat, 1 is standing, 2 is capstone
let active_tile = null;
let started_move = 0; // 0 false, 1 true
const stoneSelector = document.getElementById("stone-selector");
const standingStoneSelector = document.getElementById("standing-stone-selector");
const capstoneSelector = document.getElementById("capstone-selector");
const endTurnButton = document.getElementById("end-turn-btn");
const newGameBtn = document.getElementById('new-game-btn');
const boardSizeInput = document.getElementById('board-size-input');
renderSquares();
updateBoard();

function renderSquares() {
    const board = document.querySelector(".board");
    board.replaceChildren();
    const boardArray = [];
    const sideLength = getBoardSize();
    let board_size = sideLength ** 2;
    board.style.gridTemplateColumns = `repeat(${sideLength}, 1fr)`;
    board.style.gridTemplateRows = `repeat(${sideLength}, 1fr)`;

    for (let i = 0; i < board_size; i++) {
        const board_tile = document.createElement("div");
        board_tile.classList.add("boardTile");
        board_tile.setAttribute("id", "boardTile-" + i);
        boardArray.push(board_tile);
    }

    boardArray.forEach((board_tile, i) => {
        board_tile.addEventListener("pointerdown", function() {
            updateBoardLeftClick(board_tile);
        });
        board_tile.addEventListener("contextmenu", function(e) {
            e.preventDefault();
            updateBoardRightClick(board_tile);
        });
    });

    boardArray.forEach((board_tile) => {
        board.append(board_tile);
    });
}

function updateBoardLeftClick(board_tile) {
    let tileId = board_tile.id.split("-")[1];
    if (isTurnOver()) {
        // highlight end turn in a pulsing red
        if (endTurnButton.classList.contains('pulse-red')) return;
        endTurnButton.classList.add('pulse-red');
        endTurnButton.addEventListener('animationend', () => {
            endTurnButton.classList.remove('pulse-red');
        }, { once: true });
        // now just return
        return;
    }
    if (active_tile) {
        started_move = 1;
        // placing stones
        if (active_tile === stoneSelector || active_tile === standingStoneSelector || active_tile === capstoneSelector) {
            let pieceType = 0;
            if (active_tile === standingStoneSelector) pieceType = 1;
            if (active_tile === capstoneSelector) pieceType = 2;

            if (Module._returnTileCount(tileId) === 0){
                Module._placeStone(tileId, current_turn, pieceType);
                active_tile.classList.remove("active_tile");
                active_tile = null;
            } else { return; }
        }

        else if (board_tile !== active_tile) {
            if (Module._getBufferCount() === 0) {
                if (Module._canPlayerSelect(tileId, current_turn)){
                    active_tile?.classList.remove("active_tile");
                    board_tile.classList.add("active_tile");
                    active_tile = board_tile;
                    Module._updateBoardLeftClick(tileId);
                }
            }
            else {
                // moving buffer
                if (Module._canMoveBufferTo(tileId)) {
                    Module._updateBoardLeftClick(tileId);
                }
                if (Module._getBufferCount() === 0) {
                    active_tile.classList.remove("active_tile");
                    active_tile = null;
                }
            }
        }
        else if (board_tile === active_tile){
            // picking up stones to move
            Module._updateBoardLeftClick(tileId);
        }
    } else {
        if (Module._canPlayerSelect(tileId, current_turn)){
            board_tile.classList.add("active_tile");
            active_tile = board_tile;
            Module._updateBoardLeftClick(tileId);
        }
    }
    updateBoard();
}

function updateBoardRightClick(board_tile) {
    // Module._placeStone(board_tile.id.split("-")[1], 1, 0);
    // updateBoard();
    // Module._updateBoardRightClick(board_tile.id.split("-")[1]);
}

function getBoardSize(){
    const result = Module._getBoardSize();
    return result;
}

function updateBoard() {
    const sideLength = getBoardSize();
    const boardSize = sideLength * sideLength;
    const bufferCount = Module._getBufferCount();

    // Get some buffer information
    let targetHoverTileId = -1;
    if (bufferCount > 0) {
        const movingToId = Module._getMovingToTileId();
        if (movingToId >= 0) {
            targetHoverTileId = movingToId;
        } else if (active_tile) {
            targetHoverTileId = parseInt(active_tile.id.split("-")[1]);
        }
    }

    // Draw board
    for (let tileId = 0; tileId < boardSize; tileId++) {
        const tileElement = document.getElementById(`boardTile-${tileId}`);
        tileElement.replaceChildren();

        const count = Module._returnTileCount(tileId);
        const stack = Module._returnTileStack(tileId);
        const topType = Module._returnTopPiece(tileId);

        const isHoverTile = tileId === targetHoverTileId && bufferCount > 0;
        const totalPieces = count + (isHoverTile ? bufferCount : 0);

        const tileWidth = tileElement.offsetWidth;
        const tileHeight = tileElement.offsetHeight;

        // Make pieces roughly 60% of the tile width
        const pieceSize = Math.floor(tileWidth * 0.4);

        // Max step is 12% of the tile's height.
        // Total stack height shouldn't exceed 75% of the tile's total height.
        const maxStep = Math.floor(tileHeight * 0.12);
        const compressedStep = Math.floor((tileHeight * 0.75) / totalPieces);

        const stepOffset = totalPieces <= 1 ? 0 : Math.min(maxStep, compressedStep);

        if (count > 0) {
            for (let level = 0; level < count; level++) {
                const piece = document.createElement("div");
                piece.classList.add("piece");

                piece.style.width = `${pieceSize}px`;
                piece.style.height = `${pieceSize}px`;

                const colorBit = (stack >> BigInt(level)) & 1n;
                piece.classList.add(colorBit === 0n ? "white-piece" : "black-piece");

                if (level === count - 1) {
                    if (topType === 1) piece.classList.add("standing-piece");
                    else if (topType === 2) piece.classList.add("capstone-piece");
                    else piece.classList.add("flat-piece");
                } else {
                    piece.classList.add("flat-piece");
                }

                const centerOffset = ((totalPieces - 1) / 2 - level) * stepOffset;
                piece.style.top = `calc(50% + ${centerOffset}px)`;
                tileElement.appendChild(piece);
            }
        }

        if (isHoverTile) {
            const buffer_stack = Module._getBufferStack();
            const buffer_top = Module._getBufferTop();

            for (let buffer_level = 0; buffer_level < bufferCount; buffer_level++) {
                const piece = document.createElement("div");
                piece.classList.add("piece", "in-buffer");

                piece.style.width = `${pieceSize}px`;
                piece.style.height = `${pieceSize}px`;

                const bufferBitIndex = bufferCount - 1 - buffer_level;
                const colorBit = (buffer_stack >> BigInt(bufferBitIndex)) & 1n;
                piece.classList.add(colorBit === 0n ? "white-piece" : "black-piece");

                if (buffer_level === bufferCount - 1) {
                    if (buffer_top === 1) piece.classList.add("standing-piece");
                    else if (buffer_top === 2) piece.classList.add("capstone-piece");
                    else piece.classList.add("flat-piece");
                } else {
                    piece.classList.add("flat-piece");
                }

                const totalLevel = count + buffer_level;
                const centerOffset = ((totalPieces - 1) / 2 - totalLevel) * stepOffset;
                piece.style.top = `calc(50% + ${centerOffset}px)`;
                tileElement.appendChild(piece);
            }
        }
    }
    updatePlayerPanel();
    if (isTurnOver()) {
        endTurnButton.classList.add("turn-over");
    } else {
        endTurnButton.classList.remove("turn-over");
    }
}

function isTurnOver() {
    return started_move === 1 && Module._getBufferCount() === 0;
}

document.querySelector(".reset-btn").addEventListener("pointerdown", () => {
    active_tile?.classList.remove("active_tile");
    active_tile = null;
    started_move = 0;
    selectedPiece = -1;
    Module._reset();
    updateBoard();
});

document.querySelector(".end-turn-btn").addEventListener("pointerdown", () => {
    if (isTurnOver()){
        active_tile?.classList.remove("active_tile");
        active_tile = null;
        started_move = 0;
        selectedPiece = -1;
        current_turn = current_turn === 0 ? 1 : 0;
        Module._endTurn();
        updateBoard();
    }
});

stoneSelector.addEventListener("pointerdown", () => {
    if (!started_move && Module._getPiecesLeft(current_turn, 0) != 0) {
        selectedPiece = 0;
        active_tile?.classList.remove("active_tile");
        stoneSelector.classList.add("active_tile");
        active_tile = stoneSelector;
    }
});

standingStoneSelector.addEventListener("pointerdown", () => {
    if (!started_move && Module._getPiecesLeft(current_turn, 0) != 0) {
        selectedPiece = 1;
        active_tile?.classList.remove("active_tile");
        standingStoneSelector.classList.add("active_tile");
        active_tile = standingStoneSelector;
    }
});

capstoneSelector.addEventListener("pointerdown", () => {
    if (!started_move && Module._getPiecesLeft(current_turn, 1) != 0) {
        selectedPiece = 2;
        active_tile?.classList.remove("active_tile");
        capstoneSelector.classList.add("active_tile");
        active_tile = capstoneSelector;
    }
});

newGameBtn.addEventListener('click', () => {
    const newSize = parseInt(boardSizeInput.value, 10);

    if (isNaN(newSize) || newSize < 3 || newSize > 8) {
        newGameBtn.classList.remove('pulse-red');
        void newGameBtn.offsetWidth;
        newGameBtn.classList.add('pulse-red');
        newGameBtn.addEventListener('animationend', () => {
            newGameBtn.classList.remove('pulse-red');
        }, { once: true });
        return;
    }

    Module._restartGame(newSize);
    current_turn = 0;
    selectedPiece = -1;
    active_tile = null;
    started_move = 0;
    Module._init();
    renderSquares();
    updateBoard();
});

function updatePlayerPanel() {
    const whiteStatus = document.getElementById("white-status");
    const blackStatus = document.getElementById("black-status");
    const stonePreview = document.getElementById("stone-preview");
    const standingStonePreview = document.getElementById("standing-stone-preview");
    const capPreview = document.getElementById("capstone-preview");

    if (current_turn === 0) {
        whiteStatus.textContent = "White's Turn";
        whiteStatus.className = "white-info active-turn-text";

        blackStatus.textContent = "Black Player";
        blackStatus.className = "black-info inactive-turn-text";

        stonePreview.className = "piece flat-piece white-piece";
        standingStonePreview.className = "piece standing-piece white-piece";
        capPreview.className = "piece capstone-piece white-piece";
    } else {
        whiteStatus.textContent = "White Player";
        whiteStatus.className = "white-info inactive-turn-text";

        blackStatus.textContent = "Black's Turn";
        blackStatus.className = "black-info active-turn-text";

        stonePreview.className = "piece flat-piece black-piece";
        standingStonePreview.className = "piece standing-piece black-piece";
        capPreview.className = "piece capstone-piece black-piece";
    }

    let remainingStones = Module._getPiecesLeft(current_turn, 0);
    let remainingCapstones = Module._getPiecesLeft(current_turn, 1);

    document.getElementById("stone-count").textContent = remainingStones;
    document.getElementById("capstone-count").textContent = remainingCapstones;
}
