import os


SERVER_HOST = os.getenv("CHESS_SERVER_HOST", "192.168.68.61")
SERVER_PORT = int(os.getenv("CHESS_SERVER_PORT", "8080"))

ENGINE_MOVE_TIME_MS = 1000

TIME_CONTROLS = {
    "Blitz 1+1": (60, 60, 1),
    "Bullet 3+0": (180, 180, 0),
    "Casual 15+10": (900, 900, 10),
    "Slow-fast 10+5 vs 1+5": (600, 60, 5),
}

DEFAULT_TIME_CONTROL = "Blitz 1+1"

BOARD_SIZE = 640
SQUARE_SIZE = BOARD_SIZE // 8

LIGHT_SQUARE = "#f0d9b5"
DARK_SQUARE = "#b58863"
SELECT_SQUARE = "#ffff66"

PIECE_FONT = ("DejaVu Sans", 48)
COORDINATE_FONT = ("DejaVu Sans", 9, "bold")

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
    "N": "pieces-png/white-knight.png",
    "B": "pieces-png/white-bishop.png",
    "R": "pieces-png/white-rook.png",
    "Q": "pieces-png/white-queen.png",
    "K": "pieces-png/white-king.png",

    "p": "pieces-png/black-pawn.png",
    "n": "pieces-png/black-knight.png",
    "b": "pieces-png/black-bishop.png",
    "r": "pieces-png/black-rook.png",
    "q": "pieces-png/black-queen.png",
    "k": "pieces-png/black-king.png",
}

USE_UCICODE_PIECES = False