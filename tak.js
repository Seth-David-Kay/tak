import createModule from "./main.js";

const Module = await createModule();

Module._init();
let current_turn = 0; // 0 is white
let selectedPiece = -1; // -1 is nothing, 0 if flat, 1 is standing, 2 is capstone
let active_tile = null;
let started_move = 0; // 0 false, 1 true
let total_moves_taken = 0;
let is_game_over = 0;
const stoneSelector = document.getElementById("stone-selector");
const standingStoneSelector = document.getElementById("standing-stone-selector");
const capstoneSelector = document.getElementById("capstone-selector");
const endTurnButton = document.getElementById("end-turn-btn");
const newGameBtn = document.getElementById('new-game-btn');
const boardSizeInput = document.getElementById('board-size-input');
const settingsBtn = document.getElementById('settings-btn');
const settingsDropdown = document.getElementById('settings-dropdown');
const botToggle = document.getElementById('bot-toggle');
const botFirstToggle = document.getElementById('bot-first-toggle');
const difficultySlider = document.getElementById("difficulty-slider");
const difficultyValue = document.getElementById("difficulty-value");
const modalNewGameBtn = document.getElementById("modal-new-game-btn");
const modalCancelBtn = document.getElementById("modal-cancel-btn");
const darkModeToggle = document.getElementById("dark-mode-toggle");
export let botEnabled  = false;
export let botColor = 1; // 0 is white (does first), 1 is black (goes second)
loadSettings();
const newSize = parseInt(boardSizeInput.value, 10) || 6;
startNewGame(newSize);

difficultySlider.addEventListener("input", () => {
    difficultyValue.textContent = difficultySlider.value;
    saveSettings();
});

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
    if (total_moves_taken < 2){
        if (Module._returnTileCount(tileId) === 0){
            started_move = 1;
            Module._placeStone(tileId, current_turn === 0 ? 1 : 0, 0);
        } else { return; }
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
        }else if (Module._returnTileCount(tileId) == 0) {
            started_move = 1;
            Module._placeStone(tileId, current_turn, 0);
        }
    }
    updateBoard();
}

function updateBoardRightClick(board_tile) {
}

function getBoardSize(){
    const result = Module._getBoardSize();
    return result;
}

function updateBoard() {
    const sideLength = getBoardSize();
    const boardSize = sideLength * sideLength;
    const bufferCount = Module._getBufferCount();

    let targetHoverTileId = -1;
    if (bufferCount > 0) {
        const movingToId = Module._getMovingToTileId();
        if (movingToId >= 0) {
            targetHoverTileId = movingToId;
        } else if (active_tile) {
            targetHoverTileId = parseInt(active_tile.id.split("-")[1]);
        }
    }

    for (let tileId = 0; tileId < boardSize; tileId++) {
        const tileElement = document.getElementById(`boardTile-${tileId}`);
        tileElement.replaceChildren();
        tileElement.classList.remove('has-buffer');

        const count   = Module._returnTileCount(tileId);
        const stack   = Module._returnTileStack(tileId);
        const topType = Module._returnTopPiece(tileId);

        const isHoverTile = tileId === targetHoverTileId && bufferCount > 0;
        if (isHoverTile) tileElement.classList.add('has-buffer');
        const totalPieces = count + (isHoverTile ? bufferCount : 0);

        const tileHeight = tileElement.offsetHeight;
        const tileWidth = tileElement.offsetWidth;
        // change liftPx to change lift height
        const liftPx = Math.round(tileHeight * 0.3);
        const targetHeight = tileHeight * 0.96;
        const naturalPiecePx = tileWidth * 0.5;
        // edit minvisible toward 0.4 for more legible, toward 0.15 if more stay same
        const minVisible = 0.15;
        // adjust 0.6 up if default further from each other, down if default closer to each other
        const maxStep = naturalPiecePx * 0.3;

        let stepOffset, pieceScale;

        if (totalPieces <= 1) {
            stepOffset = 0;
            pieceScale = 1;
        } else {
            const naturalStep = Math.min(maxStep, (targetHeight - naturalPiecePx) / (totalPieces - 1));

            if (naturalStep >= minVisible * naturalPiecePx) {
                pieceScale = 1;
                stepOffset = naturalStep;
            } else {
                const piecePx = targetHeight / (1 + (totalPieces - 1) * minVisible);
                pieceScale = Math.max(0.1, piecePx / naturalPiecePx);
                stepOffset = (targetHeight - piecePx) / (totalPieces - 1);
            }
        }

        tileElement.style.setProperty('--piece-scale', pieceScale);

        if (count > 0) {
            for (let level = 0; level < count; level++) {
                const piece = document.createElement("div");
                piece.classList.add("piece");

                const colorBit = (stack >> BigInt(level)) & 1n;
                piece.classList.add(colorBit === 0n ? "white-piece" : "black-piece");

                if (level === count - 1) {
                    if (topType === 1)      piece.classList.add("standing-piece");
                    else if (topType === 2) piece.classList.add("capstone-piece");
                    else                    piece.classList.add("flat-piece");
                } else {
                    piece.classList.add("flat-piece");
                }

                const centerOffset = ((totalPieces - 1) / 2 - level) * stepOffset;
                piece.style.top = `calc(50% + ${centerOffset}px)`;
                if (pieceScale < 1) piece.style.border = 'none';
                tileElement.appendChild(piece);
            }
        }

        if (isHoverTile) {
            const buffer_stack = Module._getBufferStack();
            const buffer_top   = Module._getBufferTop();

            for (let buffer_level = 0; buffer_level < bufferCount; buffer_level++) {
                const piece = document.createElement("div");
                piece.classList.add("piece", "in-buffer");
                piece.style.setProperty('--lift', `-${liftPx}px`);

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
                if (pieceScale < 1) piece.style.border = 'none';
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

const delay = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
async function botMove() {
    document.querySelectorAll('.board, .picker, .buttons').forEach(el => el.style.pointerEvents = 'none');
    const movePtr = Module._getBotMove(current_turn);
    if (movePtr === 0) {
        console.log("No legal moves available!");
        return;
    }
    const clickCount = Module.HEAP32[movePtr >> 2];
    const clickSequence = [];
    for (let i = 0; i < clickCount; i++) {
        clickSequence.push(Module.HEAP32[(movePtr >> 2) + 1 + i]);
    }
    const extraData = Module.HEAP32[(movePtr >> 2) + 1 + clickCount];
    if (extraData === -1) {
        const sourceTileId = clickSequence[0];
        const carryCount = clickSequence.length - 1;

        const sourceElement = document.getElementById(`boardTile-${sourceTileId}`);
        if (sourceElement) {
            for (let c = 0; c < carryCount; c++) {
                updateBoardLeftClick(sourceElement);
                await delay(300);
            }
        }

        // Now walk the drop squares, one click per piece dropped
        for (let i = 1; i < clickSequence.length; i++) {
            const tileId = clickSequence[i];
            const tileElement = document.getElementById(`boardTile-${tileId}`);
            if (tileElement) {
                updateBoardLeftClick(tileElement);
                await delay(350);
            }
        }
    } else {
        const targetTileId = clickSequence[0];
        const tileElement = document.getElementById(`boardTile-${targetTileId}`);
        if (tileElement) {
            if (total_moves_taken >= 2) {
                if (extraData === 0) stoneSelector.dispatchEvent(new PointerEvent('pointerdown'));
                else if (extraData === 1) standingStoneSelector.dispatchEvent(new PointerEvent('pointerdown'));
                else if (extraData === 2) capstoneSelector.dispatchEvent(new PointerEvent('pointerdown'));
            }
            updateBoardLeftClick(tileElement);
        }
    }
    const endTurnBtn = document.querySelector(".end-turn-btn");
    if (endTurnBtn) {
        endTurnBtn.dispatchEvent(new PointerEvent('pointerdown'));
    }
    document.querySelectorAll('.board, .picker, .buttons').forEach(el => el.style.pointerEvents = 'auto');
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
        let isGameOver = Module._isGameOver();
        if (isGameOver != 0 && is_game_over == 0){
            is_game_over = 1;
            if (isGameOver == 1) {
                const whiteStatus = document.getElementById("white-status");
                whiteStatus.textContent = "White Wins";
                triggerGameOverPopup("White");
            }else {
                const blackStatus = document.getElementById("black-status");
                blackStatus.textContent = "Black Wins";
                triggerGameOverPopup("Black");
            }
        }
        active_tile?.classList.remove("active_tile");
        active_tile = null;
        started_move = 0;
        selectedPiece = -1;
        current_turn = current_turn === 0 ? 1 : 0;
        total_moves_taken += 1;
        Module._endTurn();
        updateBoard();
        if (botEnabled) {
            if (botColor == current_turn && !is_game_over) {
                setTimeout(() => { botMove(); }, 100);
            }
        }
    }
});

stoneSelector.addEventListener("pointerdown", () => {
    if (!started_move && Module._getPiecesLeft(current_turn, 0) != 0 && total_moves_taken >= 2) {
        selectedPiece = 0;
        active_tile?.classList.remove("active_tile");
        stoneSelector.classList.add("active_tile");
        active_tile = stoneSelector;
    }
});

standingStoneSelector.addEventListener("pointerdown", () => {
    if (!started_move && Module._getPiecesLeft(current_turn, 0) != 0 && total_moves_taken >= 2) {
        selectedPiece = 1;
        active_tile?.classList.remove("active_tile");
        standingStoneSelector.classList.add("active_tile");
        active_tile = standingStoneSelector;
    }
});

capstoneSelector.addEventListener("pointerdown", () => {
    if (!started_move && Module._getPiecesLeft(current_turn, 1) != 0 && total_moves_taken >= 2) {
        selectedPiece = 2;
        active_tile?.classList.remove("active_tile");
        capstoneSelector.classList.add("active_tile");
        active_tile = capstoneSelector;
    }
});

function startNewGame(board_size) {
    Module._restartGame(board_size);
    current_turn = 0;
    selectedPiece = -1;
    active_tile = null;
    started_move = 0;
    total_moves_taken = 0;
    is_game_over = 0;
    Module._init();
    Module._setSearchDepth(difficultyValue.textContent);
    renderSquares();
    updateBoard();
    if (botEnabled && botColor == 0) setTimeout(() => { botMove(); }, 100);
}

newGameBtn.addEventListener('click', () => {
    const newSize = parseInt(boardSizeInput.value, 10);
    if (isNaN(newSize) || newSize < 3 || newSize > 10) {
        newGameBtn.classList.remove('pulse-red');
        void newGameBtn.offsetWidth;
        newGameBtn.classList.add('pulse-red');
        newGameBtn.addEventListener('animationend', () => {
            newGameBtn.classList.remove('pulse-red');
        }, { once: true });
        return;
    }
    startNewGame(newSize);
    settingsDropdown.classList.remove('open');
    saveSettings();
});

modalCancelBtn.addEventListener("pointerdown", () => {
    document.getElementById("win-modal").classList.remove("open");
});

modalNewGameBtn.addEventListener("pointerdown", () => {
    const newSize = parseInt(boardSizeInput.value, 10);
    if (isNaN(newSize) || newSize < 3 || newSize > 10) {
        modalNewGameBtn.classList.remove('pulse-red');
        void modalNewGameBtn.offsetWidth;
        modalNewGameBtn.classList.add('pulse-red');
        modalNewGameBtn.addEventListener('animationend', () => {
            modalNewGameBtn.classList.remove('pulse-red');
        }, { once: true });
        return;
    }
    startNewGame(newSize);
    document.getElementById("win-modal").classList.remove("open");
});

function triggerGameOverPopup(winnerName) {
    const winModal = document.getElementById("win-modal");
    const winMessage = document.getElementById("win-message");

    winMessage.textContent = `${winnerName} Wins!`;
    winModal.classList.add("open");
}

settingsBtn.addEventListener('pointerdown', (e) => {
    e.stopPropagation();
    settingsDropdown.classList.toggle('open');
});

document.addEventListener('pointerdown', (e) => {
    if (!settingsDropdown.contains(e.target) && e.target !== settingsBtn) {
        settingsDropdown.classList.remove('open');
    }
});

botToggle.addEventListener('pointerdown', (e) => {
    e.stopPropagation();
    botEnabled = !botEnabled;
    botToggle.classList.toggle('active', botEnabled);
    saveSettings();
});

botFirstToggle.addEventListener('pointerdown', (e) => {
    e.stopPropagation();
    botColor = botColor == 0 ? 1 : 0;
    botFirstToggle.classList.toggle('active', !botColor);
    saveSettings();
});

darkModeToggle.addEventListener("pointerdown", (e) => {
    e.stopPropagation();
    const targetToggle = e.currentTarget;
    targetToggle.classList.toggle("active");
    document.body.classList.toggle("dark-theme");
    saveSettings();
});

function saveSettings() {
    const settings = {
        boardSize: boardSizeInput.value,
        botEnabled: botToggle.classList.contains("active"),
        botFirst: botFirstToggle.classList.contains("active"),
        difficulty: difficultySlider.value,
        darkMode: darkModeToggle.classList.contains("active"),
        boardSize: boardSizeInput.value
    };

    localStorage.setItem("tak_game_settings", JSON.stringify(settings));
}

function loadSettings() {
    const savedData = localStorage.getItem("tak_game_settings");
    if (!savedData) return;

    try {
        const settings = JSON.parse(savedData);
        if (settings.boardSize) {
            boardSizeInput.value = settings.boardSize;
        }
        if (settings.botEnabled !== undefined) {
            botEnabled = settings.botEnabled;
            botToggle.classList.toggle("active", botEnabled);
        }
        if (settings.botFirst !== undefined) {
            botColor = settings.botFirst ? 0 : 1;
            botFirstToggle.classList.toggle("active", settings.botFirst);
        }
        if (settings.difficulty !== undefined) {
            difficultySlider.value = settings.difficulty;
            if (difficultyValue) difficultyValue.textContent = settings.difficulty;
        }
        darkModeToggle.classList.toggle("active", settings.darkMode);
        document.body.classList.toggle("dark-theme", settings.darkMode);
    } catch (e) {
    }
}

function updatePlayerPanel() {
    const whiteStatus = document.getElementById("white-status");
    const blackStatus = document.getElementById("black-status");
    const stonePreview = document.getElementById("stone-preview");
    const standingStonePreview = document.getElementById("standing-stone-preview");
    const capPreview = document.getElementById("capstone-preview");

    // Calculate remaining counts for the active player
    let remainingStones = Module._getPiecesLeft(current_turn, 0);
    let remainingCapstones = Module._getPiecesLeft(current_turn, 1);

    // Calculate remaining pieces for the WAITING (other) player
    let otherTurn = 1 - current_turn;
    let otherStones = Module._getPiecesLeft(otherTurn, 0);
    let otherCapstones = Module._getPiecesLeft(otherTurn, 1);
    let otherTotalLeft = otherStones + otherCapstones;

    if (current_turn === 0) {
        if (!is_game_over) {
            whiteStatus.innerHTML = "White's Turn";
            whiteStatus.className = "white-info active-turn-text";

            // Show Black's total inventory below their name
            blackStatus.innerHTML = `Black Player<br><span class="count-subtext">${otherTotalLeft} left</span>`;
            blackStatus.className = "black-info inactive-turn-text";
        }

        stonePreview.className = "piece flat-piece white-piece";
        standingStonePreview.className = "piece standing-piece white-piece";
        capPreview.className = "piece capstone-piece white-piece";
    } else {
        if (!is_game_over) {
            // Show White's total inventory below their name
            whiteStatus.innerHTML = `White Player<br><span class="count-subtext">${otherTotalLeft} left</span>`;
            whiteStatus.className = "white-info inactive-turn-text";

            blackStatus.innerHTML = "Black's Turn";
            blackStatus.className = "black-info active-turn-text";
        }

        stonePreview.className = "piece flat-piece black-piece";
        standingStonePreview.className = "piece standing-piece black-piece";
        capPreview.className = "piece capstone-piece black-piece";
    }

    document.getElementById("stone-count").textContent = remainingStones;
    document.getElementById("capstone-count").textContent = remainingCapstones;
}
