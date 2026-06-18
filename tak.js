import createModule from "./main.js";

const Module = await createModule();

renderSquares();

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
        board_tile.setAttribute("id", "boardTile" + i);
        boardArray.push(board_tile);
    }

    boardArray.forEach((board_tile, i) => {
        determineSquareColor(board_tile, i);
    });

    boardArray.forEach((board_tile, i) => {
        board_tile.addEventListener("click", function() {
            console.log(board_tile);
            updateBoard(board_tile);
        });
    });

    boardArray.forEach((board_tile) => {
        board.append(board_tile);
    });
}

function updateBoard(board_tile) {

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
