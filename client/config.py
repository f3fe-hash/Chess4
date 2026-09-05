SERVER_HOST = "127.0.0.1"
SERVER_PORT = 8080

ENGINE_MOVE_TIME_MS = 1000

BOARD_SIZE = 640
SQUARE_SIZE = BOARD_SIZE // 8

LIGHT_SQUARE = "#f0d9b5"
DARK_SQUARE = "#b58863"
SELECT_SQUARE = "#ffff66"

PIECE_FONT = ("DejaVu Sans", 48)
COORDINATE_FONT = ("DejaVu Sans", 9)

UNICODE_PIECES = {
    "P": "♙",
    "N": "♘",
    "B": "♗",
    "R": "♖",
    "Q": "♕",
    "K": "♔",

    "p": "♟",
    "n": "♞",
    "b": "♝",
    "r": "♜",
    "q": "♛",
    "k": "♚",
}

PIECES_TO_FILE = {
    "P": "pieces-png/white-pawn.png",
    "N": "pieces-png/white-pawn.png",
    "B": "pieces-png/white-pawn.png",
    "R": "pieces-png/white-pawn.png",
    "Q": "pieces-png/white-pawn.png",
    "K": "pieces-png/white-pawn.png",

    "p": "pieces-png/black-pawn.png",
    "n": "pieces-png/black-pawn.png",
    "b": "pieces-png/black-pawn.png",
    "r": "pieces-png/black-pawn.png",
    "q": "pieces-png/black-pawn.png",
    "k": "pieces-png/black-pawn.png",
}

USE_UCICODE_PIECES = True