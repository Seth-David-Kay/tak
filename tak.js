import createModule from "./main.js";

const Module = await createModule();

Module._init();
let active_tile = null;
let current_turn = 0; // 0 is white
renderSquares();
updateBoard();

function renderSquares() {
    const board = document.querySelector(".board");
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
        determineSquareColor(board_tile, i);
    });

    boardArray.forEach((board_tile, i) => {
        board_tile.addEventListener("click", function() {
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
    if (active_tile === board_tile) {
        let tileId = board_tile.id.split("-")[1];
        Module._updateBoardLeftClick(tileId);
        return;
    }
    active_tile?.classList.remove("active_tile");
    board_tile.classList.add("active_tile");
    active_tile = board_tile;
}

function updateBoardRightClick(board_tile) {
    Module._placeStone(board_tile.id.split("-")[1], 0, 1);
    updateBoard();
    // Module._updateBoardRightClick(board_tile.id.split("-")[1]);
}

function determineSquareColor(board_tile, i) {
    const sideLength = getBoardSize();
    const row = Math.floor(i / sideLength);
    const col = i % sideLength;

    if ((row + col) % 2 === 0) {
        board_tile.classList.add("light-tile");
    } else {
        board_tile.classList.add("dark-tile");
    }
}

function getBoardSize(){
    const result = Module._getBoardSize();
    return result;
}

function updateBoard() {
    const sideLength = getBoardSize();
    const boardSize = sideLength * sideLength;

    for (let tileId = 0; tileId < boardSize; tileId++) {
        const tileElement = document.getElementById(`boardTile-${tileId}`);

        // Remove old pieces
        tileElement.replaceChildren();

        const count = Module._returnTileCount(tileId);

        if (count === 0) continue;

        const stack = Module._returnTileStack(tileId);
        const topType = Module._returnTopPiece(tileId);

        for (let level = 0; level < count; level++) {
            const piece = document.createElement("div");

            piece.classList.add("piece");

            // Color
            const colorBit = (stack >> BigInt(level)) & 1n;

            if (colorBit === 0n) {
                piece.classList.add("white-piece");
            } else {
                piece.classList.add("black-piece");
            }

            // Shape
            if (level === count - 1) {
                if (topType === 1) { // STANDING
                    piece.classList.add("standing-piece");
                }
                else if (topType === 2) { // CAP
                    piece.classList.add("capstone-piece");
                }
                else { // FLAT
                    piece.classList.add("flat-piece");
                }
            } else {
                piece.classList.add("flat-piece");
            }

            // Stack pieces upward
            const offset = level * 8;
            piece.style.top = `calc(50% - ${offset}px)`;

            tileElement.appendChild(piece);
        }
    }
    updatePlayerPanel();
}

document.querySelector(".reset-btn").addEventListener("click", () => {
    // Module._resetGame();
    current_turn = 0;
    active_tile = null;
    updateBoard();
});

document.querySelector(".end-turn-btn").addEventListener("click", () => {
    current_turn = current_turn === 0 ? 1 : 0;
    updateBoard();
});


function updatePlayerPanel() {
    const whiteStatus = document.getElementById("white-status");
    const blackStatus = document.getElementById("black-status");
    const stonePreview = document.getElementById("stone-preview");
    const capPreview = document.getElementById("capstone-preview");

    if (current_turn === 0) {
        whiteStatus.textContent = "White's Turn";
        whiteStatus.className = "white-info active-turn-text";

        blackStatus.textContent = "Black Player";
        blackStatus.className = "black-info inactive-turn-text";

        stonePreview.className = "piece flat-piece white-piece";
        capPreview.className = "piece capstone-piece white-piece";
    } else {
        whiteStatus.textContent = "White Player";
        whiteStatus.className = "white-info inactive-turn-text";

        blackStatus.textContent = "Black's Turn";
        blackStatus.className = "black-info active-turn-text";

        stonePreview.className = "piece flat-piece black-piece";
        capPreview.className = "piece capstone-piece black-piece";
    }

    // Update Piece Counts Here
    // call module.getpiececounts and set our counts equal to those
    let remainingStones = Module._returnPiecesLeft(current_turn, 0);
    let remainingCapstones = Module._returnPiecesLeft(current_turn, 1);

    document.getElementById("stone-count").textContent = remainingStones;
    document.getElementById("capstone-count").textContent = remainingCapstones;
}
