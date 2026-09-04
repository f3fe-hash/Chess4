import socket
import threading
import tkinter as tk
from tkinter import messagebox

from board import ChessBoard


# ============================================================
# Configuration
# ============================================================

SERVER_HOST = "127.0.0.1"
SERVER_PORT = 8080

# How long the engine is allowed to think for each move.
ENGINE_MOVE_TIME_MS = 1000

BOARD_SIZE = 640
SQUARE_SIZE = BOARD_SIZE // 8

LIGHT_SQUARE = "#f0d9b5"
DARK_SQUARE = "#b58863"
HIGHLIGHT_SQUARE = "#f6f669"
SELECT_SQUARE = "#ffff66"

PIECE_FONT = ("DejaVu Sans", 48)

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

# ============================================================
# UCI Client
# ============================================================

class UCIClient:
    def __init__(self, host, port, on_bestmove, on_status):
        self.host = host
        self.port = port

        self.on_bestmove = on_bestmove
        self.on_status = on_status

        self.socket = None
        self.reader = None

        self.connected = False

        self.receive_thread = None

        self.waiting_for_bestmove = False

    # --------------------------------------------------------
    # Connect
    # --------------------------------------------------------

    def connect(self):
        try:
            self.socket = socket.socket(
                socket.AF_INET,
                socket.SOCK_STREAM
            )

            self.socket.settimeout(None)

            self.socket.connect(
                (self.host, self.port)
            )

            self.reader = self.socket.makefile(
                "r",
                encoding="utf-8",
                newline="\n"
            )

            self.connected = True

            self.on_status(
                f"Connected to {self.host}:{self.port}"
            )

            # Start receiving responses.
            self.receive_thread = threading.Thread(
                target=self.receive_loop,
                daemon=True
            )

            self.receive_thread.start()

            # UCI initialization.
            self.send("uci")

            return True

        except Exception as e:
            self.on_status(f"Connection failed: {e}")
            return False

    # --------------------------------------------------------
    # Receive loop
    # --------------------------------------------------------

    def receive_loop(self):
        try:
            while self.connected:
                line = self.reader.readline()

                if not line:
                    break

                line = line.strip()

                if not line:
                    continue

                print(f"<< {line}")

                # UCI handshake.
                if line == "uciok":
                    self.send("isready")

                elif line == "readyok":
                    self.on_status("Engine ready.")

                elif line.startswith("bestmove"):
                    parts = line.split()

                    if len(parts) >= 2:
                        bestmove = parts[1]

                        # Tkinter calls must happen on the main thread.
                        self.on_bestmove(bestmove)

        except Exception as e:
            if self.connected:
                self.on_status(f"Network error: {e}")

        finally:
            self.connected = False
            self.on_status("Disconnected.")

    # --------------------------------------------------------
    # Send command
    # --------------------------------------------------------

    def send(self, command):
        if not self.connected:
            return False

        try:
            print(f">> {command}")

            self.socket.sendall(
                (command + "\n").encode("utf-8")
            )

            return True

        except Exception as e:
            self.on_status(f"Send error: {e}")
            return False

    # --------------------------------------------------------
    # Send current position
    # --------------------------------------------------------

    def send_position(self, moves):
        command = "position startpos"

        if moves:
            command += " moves " + " ".join(moves)

        return self.send(command)

    # --------------------------------------------------------
    # Ask engine to move
    # --------------------------------------------------------

    def go(self):
        self.waiting_for_bestmove = True

        return self.send(
            f"go movetime {ENGINE_MOVE_TIME_MS}"
        )

    # --------------------------------------------------------
    # Disconnect
    # --------------------------------------------------------

    def close(self):
        self.connected = False

        try:
            if self.reader:
                self.reader.close()
        except Exception:
            pass

        try:
            if self.socket:
                self.socket.close()
        except Exception:
            pass


# ============================================================
# Tkinter GUI
# ============================================================

class ChessGUI:
    def __init__(self, root):
        self.root = root

        self.root.title("UCI Chess Client")

        self.board = ChessBoard()

        self.dragging = False
        self.drag_piece = None
        self.drag_from = None

        self.engine_thinking = False

        # ----------------------------------------------------
        # Board
        # ----------------------------------------------------

        self.canvas = tk.Canvas(
            root,
            width=BOARD_SIZE,
            height=BOARD_SIZE,
            highlightthickness=0
        )

        self.canvas.pack(
            padx=10,
            pady=(10, 5)
        )

        self.canvas.bind(
            "<ButtonPress-1>",
            self.on_mouse_down
        )

        self.canvas.bind(
            "<B1-Motion>",
            self.on_mouse_drag
        )

        self.canvas.bind(
            "<ButtonRelease-1>",
            self.on_mouse_up
        )

        # ----------------------------------------------------
        # Status
        # ----------------------------------------------------

        self.status_var = tk.StringVar(
            value="Connecting..."
        )

        self.status_label = tk.Label(
            root,
            textvariable=self.status_var,
            anchor="w"
        )

        self.status_label.pack(
            fill="x",
            padx=10,
            pady=(0, 10)
        )

        # ----------------------------------------------------
        # Controls
        # ----------------------------------------------------

        self.control_frame = tk.Frame(root)
        self.control_frame.pack(
            fill="x",
            padx=10,
            pady=(0, 10)
        )

        self.new_game_button = tk.Button(
            self.control_frame,
            text="New Game",
            command=self.new_game
        )

        self.new_game_button.pack(
            side="left"
        )

        # ----------------------------------------------------
        # UCI client
        # ----------------------------------------------------

        self.client = UCIClient(
            SERVER_HOST,
            SERVER_PORT,
            self.engine_bestmove,
            self.set_status
        )

        self.draw_board()

        # Connect after Tkinter starts.
        self.root.after(
            100,
            self.connect
        )

        self.root.protocol(
            "WM_DELETE_WINDOW",
            self.close
        )

    def new_game(self):
        if self.engine_thinking:
            return

        self.board.reset()

        self.draw_board()

        self.set_status("Starting new game...")

        if not self.client.send("ucinewgame"):
            messagebox.showerror(
                "Connection Error",
                "Failed to send ucinewgame to the UCI server."
            )
            return

        if not self.client.send_position(
            self.board.move_history
        ):
            messagebox.showerror(
                "Connection Error",
                "Failed to reset the UCI server position."
            )
            return

        self.set_status("New game. Your turn.")

    # --------------------------------------------------------
    # Connection
    # --------------------------------------------------------

    def connect(self):
        if self.client.connect():
            self.set_status(
                "Connected. Initializing UCI..."
            )
        else:
            self.set_status(
                "Unable to connect."
            )

    # --------------------------------------------------------
    # Status
    # --------------------------------------------------------

    def set_status(self, text):
        # Ensure Tkinter is modified from its own thread.
        self.root.after(
            0,
            lambda: self.status_var.set(text)
        )

    # --------------------------------------------------------
    # Draw board
    # --------------------------------------------------------

    def draw_board(self):
        self.canvas.delete("all")

        for row in range(8):
            for col in range(8):
                x1 = col * SQUARE_SIZE
                y1 = row * SQUARE_SIZE

                x2 = x1 + SQUARE_SIZE
                y2 = y1 + SQUARE_SIZE

                if (row + col) % 2 == 0:
                    square_color = LIGHT_SQUARE
                else:
                    square_color = DARK_SQUARE

                # Highlight selected square.
                if self.drag_from is not None:
                    selected_row, selected_col = (
                        self.board.square_to_coords(
                            self.drag_from
                        )
                    )

                    if (
                        row == selected_row
                        and col == selected_col
                    ):
                        square_color = SELECT_SQUARE

                self.canvas.create_rectangle(
                    x1,
                    y1,
                    x2,
                    y2,
                    fill=square_color,
                    outline=""
                )

                piece = self.board.board[row][col]

                if piece is not None:
                    self.canvas.create_text(
                        x1 + SQUARE_SIZE // 2,
                        y1 + SQUARE_SIZE // 2,
                        text=UNICODE_PIECES[piece],
                        font=PIECE_FONT,
                        fill="black",
                        tags="piece"
                    )

        # Board coordinates.
        for col in range(8):
            file = chr(ord("a") + col)

            self.canvas.create_text(
                col * SQUARE_SIZE + 5,
                BOARD_SIZE - 5,
                text=file,
                anchor="sw",
                font=("DejaVu Sans", 9),
                fill=(
                    DARK_SQUARE
                    if col % 2 == 0
                    else LIGHT_SQUARE
                )
            )

        for row in range(8):
            rank = str(8 - row)

            self.canvas.create_text(
                5,
                row * SQUARE_SIZE + 5,
                text=rank,
                anchor="nw",
                font=("DejaVu Sans", 9),
                fill=(
                    DARK_SQUARE
                    if row % 2 == 0
                    else LIGHT_SQUARE
                )
            )

    # --------------------------------------------------------
    # Mouse -> board square
    # --------------------------------------------------------

    def mouse_to_square(self, x, y):
        if not (
            0 <= x < BOARD_SIZE
            and 0 <= y < BOARD_SIZE
        ):
            return None

        col = x // SQUARE_SIZE
        row = y // SQUARE_SIZE

        return self.board.coords_to_square(
            row,
            col
        )

    # --------------------------------------------------------
    # Mouse down
    # --------------------------------------------------------

    def on_mouse_down(self, event):
        if self.engine_thinking:
            return

        # Human plays White.
        if self.board.turn != "w":
            return

        square = self.mouse_to_square(
            event.x,
            event.y
        )

        if square is None:
            return

        row, col = self.board.square_to_coords(
            square
        )

        piece = self.board.board[row][col]

        if piece is None:
            return

        if piece.islower():
            return

        self.dragging = True
        self.drag_from = square
        self.drag_piece = piece

        self.draw_board()

    # --------------------------------------------------------
    # Drag
    # --------------------------------------------------------

    def on_mouse_drag(self, event):
        if not self.dragging:
            return

        # Redraw board and draw the piece underneath cursor.
        self.draw_board()

        self.canvas.create_text(
            event.x,
            event.y,
            text=UNICODE_PIECES[self.drag_piece],
            font=PIECE_FONT,
            fill="black"
        )

    # --------------------------------------------------------
    # Mouse up
    # --------------------------------------------------------

    def on_mouse_up(self, event):
        if not self.dragging:
            return

        # Save the source square before clearing the drag state.
        from_square = self.drag_from

        # Determine the destination from the mouse position.
        to_square = self.mouse_to_square(
            event.x,
            event.y
        )

        self.dragging = False
        self.drag_from = None

        # --------------------------------------------------------
        # Validate mouse destination.
        # --------------------------------------------------------

        if from_square is None or to_square is None:
            self.drag_piece = None
            self.draw_board()
            return

        # Dropped on the same square.
        if from_square == to_square:
            self.drag_piece = None
            self.draw_board()
            return

        # --------------------------------------------------------
        # Get the piece from the source square.
        # --------------------------------------------------------

        from_row, from_col = self.board.square_to_coords(
            from_square
        )

        piece = self.board.board[from_row][from_col]

        if piece is None:
            self.drag_piece = None
            self.draw_board()
            return

        # --------------------------------------------------------
        # Human can only play White.
        # --------------------------------------------------------

        if piece != piece.upper():
            self.drag_piece = None
            self.draw_board()
            return

        # It must actually be White's turn.
        if self.board.turn != "w":
            self.drag_piece = None
            self.draw_board()
            return

        # --------------------------------------------------------
        # Promotion.
        # --------------------------------------------------------

        promotion = None

        if piece == "P" and to_square[1] == "8":
            promotion = self.choose_promotion()

            # User cancelled promotion.
            if promotion is None:
                self.drag_piece = None
                self.draw_board()
                return

        # --------------------------------------------------------
        # Construct the UCI move.
        # --------------------------------------------------------

        uci_move = (
            from_square +
            to_square +
            (promotion if promotion else "")
        )

        # --------------------------------------------------------
        # Check whether the move is actually legal.
        #
        # This checks:
        #   - Piece movement rules
        #   - Captures
        #   - Pawns
        #   - En passant
        #   - Castling
        #   - Promotion
        #   - Check
        #   - Pins
        #   - Moving the king into check
        # --------------------------------------------------------

        if not self.board.is_legal_move(uci_move):
            self.drag_piece = None
            self.draw_board()

            self.set_status(
                f"Illegal move: {uci_move}"
            )

            return

        # --------------------------------------------------------
        # Apply the legal move locally.
        # --------------------------------------------------------

        if not self.board.make_uci_move(uci_move):
            self.drag_piece = None
            self.draw_board()

            self.set_status(
                f"Failed to apply move: {uci_move}"
            )

            return

        self.drag_piece = None

        self.draw_board()

        # --------------------------------------------------------
        # Check whether the human move ended the game.
        # --------------------------------------------------------

        if self.board.is_checkmate():
            messagebox.showinfo(
                "Checkmate",
                "Checkmate! You win."
            )

            self.set_status(
                "Checkmate. You win!"
            )

            return

        if self.board.is_stalemate():
            messagebox.showinfo(
                "Stalemate",
                "Draw by stalemate."
            )

            self.set_status(
                "Stalemate."
            )

            return

        # --------------------------------------------------------
        # Send the complete position to the engine.
        # --------------------------------------------------------

        if not self.client.send_position(
            self.board.move_history
        ):
            self.engine_thinking = False

            messagebox.showerror(
                "Connection Error",
                "Failed to send position to the UCI server."
            )

            return

        # --------------------------------------------------------
        # Ask the engine to move.
        # --------------------------------------------------------

        self.engine_thinking = True

        self.set_status(
            f"You played {uci_move}. Engine thinking..."
        )

        if not self.client.go():
            self.engine_thinking = False

            self.set_status(
                "Failed to start engine search."
            )


    # --------------------------------------------------------
    # Promotion dialog
    # --------------------------------------------------------

    def choose_promotion(self):
        result = {
            "piece": None
        }

        dialog = tk.Toplevel(self.root)

        dialog.title("Promote Pawn")

        dialog.transient(self.root)
        dialog.grab_set()

        tk.Label(
            dialog,
            text="Choose promotion:"
        ).pack(
            padx=20,
            pady=(15, 10)
        )

        frame = tk.Frame(dialog)
        frame.pack(
            padx=10,
            pady=(0, 15)
        )

        pieces = [
            ("Queen", "q"),
            ("Rook", "r"),
            ("Bishop", "b"),
            ("Knight", "n"),
        ]

        def select(piece):
            result["piece"] = piece
            dialog.destroy()

        for name, piece in pieces:
            tk.Button(
                frame,
                text=name,
                width=10,
                command=lambda p=piece: select(p)
            ).pack(
                side="left",
                padx=3
            )

        self.root.wait_window(dialog)

        return result["piece"]

    # --------------------------------------------------------
    # Engine bestmove
    # --------------------------------------------------------

    def engine_bestmove(self, bestmove):
        def apply_move():
            self.engine_thinking = False

            if bestmove in ("0000", "(none)", "none"):
                self.engine_thinking = False

                messagebox.showinfo(
                    "Game Over",
                    "The engine has no legal moves."
                )

                self.set_status("Game over.")
                return

            if not self.board.make_uci_move(bestmove):
                self.set_status(
                    f"Engine returned invalid move: {bestmove}"
                )
                return

            self.draw_board()

            self.set_status(
                f"Engine played {bestmove}. Your turn."
            )

        self.root.after(
            0,
            apply_move
        )

    # --------------------------------------------------------
    # Close
    # --------------------------------------------------------

    def close(self):
        self.client.close()
        self.root.destroy()


# ============================================================
# Main
# ============================================================

def main():
    root = tk.Tk()

    ChessGUI(root)

    root.mainloop()


if __name__ == "__main__":
    main()
