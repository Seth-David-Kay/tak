import createModule from "./main.js";

const Module = await createModule();

renderSquares();

function renderSquares() {
    const board = document.querySelector(".board");
    const boardArray = [];
    let board_size = getBoardSize() ** 2;

    for (let i = 0; i < 64; i++) {
        const board_tile = document.createElement("div");
        board_tile.classList.add("boardTile");
        boardArray.push(board_tile);
    }

    boardArray.forEach((element, i) => {
        determineSquareColor(element, i);
    });

    boardArray.forEach((element) => {
        board.append(element);
    });
}

function determineSquareColor(element, i){
    if ((i >= 0 && i <= 7) || (i >= 16 && i <= 23) || (i >= 32 && i <= 39) || (i >= 48 && i <= 55)){
        return i % 2 === 0 ? element.style.backgroundColor = "#FFF" : element.style.backgroundColor = "#000";
    }
    if ((i >= 8 && i <= 15) || (i >= 24 && i <= 31) || (i >= 40 && i <= 47) || (i >= 56 && i <= 63)){
        return i % 2 !== 0 ? element.style.backgroundColor = "#FFF" : element.style.backgroundColor = "#000";
    }
}

function getBoardSize(){
    const result = Module._getBoardSize();
    return result;
}

document.getElementById("button").onclick = () => {
    const result = Module._add(3, 3);
    document.getElementById("text").innerText = result;
};

document.getElementById("button2").onclick = () => {
    const result = Module._sub(3, 3);
    document.getElementById("text2").innerText = result;
};
