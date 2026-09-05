import tkinter as tk
from tkinter import messagebox

from board import ChessBoard
from config import (
    BOARD_SIZE,
    SQUARE_SIZE,
    LIGHT_SQUARE,
    DARK_SQUARE,
    SELECT_SQUARE,
    USE_UCICODE_PIECES,
    COORDINATE_FONT,
)

if USE_UCICODE_PIECES:
    from config import (
        UNICODE_PIECES,
        PIECE_FONT
    )
else:
    from config import (
        PIECES_TO_FILE
    )

from uci_client import UCIClient

class ChessGUI:
    def __init__(self, root, server_host, server_port):
        self.root = root

        self.root.title("UCI Chess Client")
        self.root.resizable(False, False)

        # ----------------------------------------------------
        # Game state
        # ----------------------------------------------------

        self.board = ChessBoard()

        self.dragging = False
        self.drag_piece = None
        self.drag_from = None

        self.engine_thinking = False
        self.game_over = False

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
            server_host,
            server_port,
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

    # ========================================================
    # Connection
    # ========================================================

    def connect(self):
        if self.client.connect():
            self.set_status(
                "Connected. Initializing UCI..."
            )
        else:
            self.set_status(
                "Unable to connect."
            )

    # ========================================================
    # Status
    # ========================================================

    def set_status(self, text):
        self.root.after(
            0,
            lambda: self.status_var.set(text)
        )

    # ========================================================
    # New Game
    # ========================================================

    def new_game(self):
        if self.engine_thinking:
            return

        if not self.client.connected:
            messagebox.showerror(
                "Connection Error",
                "The UCI server is not connected."
            )
            return

        self.board.reset()

        self.dragging = False
        self.drag_piece = None
        self.drag_from = None

        self.engine_thinking = False
        self.game_over = False

        self.draw_board()

        if not self.client.new_game():
            messagebox.showerror(
                "Connection Error",
                "Failed to send ucinewgame."
            )
            return

        if not self.client.send_position(
            self.board.move_history
        ):
            messagebox.showerror(
                "Connection Error",
                "Failed to reset the engine position."
            )
            return

        self.set_status(
            "New game. Your turn."
        )

    # ========================================================
    # Drawing
    # ========================================================

    def draw_board(self):
        self.canvas.delete("all")

        selected_coords = None

        if self.drag_from is not None:
            selected_coords = (
                self.board.square_to_coords(
                    self.drag_from
                )
            )

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

                if selected_coords is not None:
                    selected_row, selected_col = selected_coords

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
                    if USE_UCICODE_PIECES:
                        self.canvas.create_text(
                            x1 + SQUARE_SIZE // 2,
                            y1 + SQUARE_SIZE // 2,
                            text=UNICODE_PIECES[piece], # type: ignore
                            font=PIECE_FONT, # type: ignore
                            fill="black"
                        )
                    else:
                        self.canvas.create_image(
                            x1 + SQUARE_SIZE // 2,
                            y1 + SQUARE_SIZE // 2,
                            image=PIECES_TO_FILE[piece] # type: ignore
                        )

        self.draw_coordinates()

    def draw_coordinates(self):
        for col in range(8):
            file = chr(ord("a") + col)

            self.canvas.create_text(
                col * SQUARE_SIZE + 5,
                BOARD_SIZE - 5,
                text=file,
                anchor="sw",
                font=COORDINATE_FONT,
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
                font=COORDINATE_FONT,
                fill=(
                    DARK_SQUARE
                    if row % 2 == 0
                    else LIGHT_SQUARE
                )
            )

    # ========================================================
    # Mouse -> Square
    # ========================================================

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

    # ========================================================
    # Mouse Down
    # ========================================================

    def on_mouse_down(self, event):
        if self.engine_thinking:
            return

        if self.game_over:
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

        # Only allow White pieces.
        if piece.islower():
            return

        self.dragging = True
        self.drag_from = square
        self.drag_piece = piece

        self.draw_board()

    # ========================================================
    # Mouse Drag
    # ========================================================

    def on_mouse_drag(self, event):
        if not self.dragging:
            return

        self.draw_board()

        if USE_UCICODE_PIECES:
            self.canvas.create_text(
                event.x,
                event.y,
                text=UNICODE_PIECES[self.drag_piece], # type: ignore
                font=PIECE_FONT, # type: ignore
                fill="black"
            )
        else:
            self.canvas.create_image(
                event.x,
                event.y,
                file=PIECES_TO_FILE[self.drag_piece] # type: ignore
            )

    # ========================================================
    # Mouse Up
    # ========================================================

    def on_mouse_up(self, event):
        if not self.dragging:
            return

        from_square = self.drag_from

        to_square = self.mouse_to_square(
            event.x,
            event.y
        )

        self.dragging = False
        self.drag_from = None

        # ----------------------------------------------------
        # Invalid destination
        # ----------------------------------------------------

        if (
            from_square is None
            or to_square is None
        ):
            self.drag_piece = None
            self.draw_board()
            return

        # Same square.
        if from_square == to_square:
            self.drag_piece = None
            self.draw_board()
            return

        # ----------------------------------------------------
        # Get source piece
        # ----------------------------------------------------

        from_row, from_col = (
            self.board.square_to_coords(
                from_square
            )
        )

        piece = self.board.board[
            from_row
        ][
            from_col
        ]

        if piece is None:
            self.drag_piece = None
            self.draw_board()
            return

        # ----------------------------------------------------
        # Only White
        # ----------------------------------------------------

        if piece.islower():
            self.drag_piece = None
            self.draw_board()
            return

        if self.board.turn != "w":
            self.drag_piece = None
            self.draw_board()
            return

        # ----------------------------------------------------
        # Promotion
        # ----------------------------------------------------

        promotion = None

        if piece == "P" and to_square[1] == "8":
            promotion = self.choose_promotion()

            if promotion is None:
                self.drag_piece = None
                self.draw_board()
                return

        # ----------------------------------------------------
        # UCI move
        # ----------------------------------------------------

        uci_move = (
            from_square
            + to_square
            + (promotion or "")
        )

        # ----------------------------------------------------
        # Legal move check
        # ----------------------------------------------------

        if not self.board.is_legal_move(uci_move):
            self.drag_piece = None
            self.draw_board()

            self.set_status(
                f"Illegal move: {uci_move}"
            )

            return

        # ----------------------------------------------------
        # Apply locally
        # ----------------------------------------------------

        if not self.board.make_uci_move(uci_move):
            self.drag_piece = None
            self.draw_board()

            self.set_status(
                f"Failed to apply move: {uci_move}"
            )

            return

        self.drag_piece = None

        self.draw_board()

        # ----------------------------------------------------
        # Game over?
        # ----------------------------------------------------

        if self.check_game_over():
            return

        # ----------------------------------------------------
        # Send position
        # ----------------------------------------------------

        if not self.client.send_position(
            self.board.move_history
        ):
            self.set_status(
                "Failed to send position to server."
            )

            self.game_over = True
            return

        # ----------------------------------------------------
        # Engine move
        # ----------------------------------------------------

        self.engine_thinking = True

        self.set_status(
            f"You played {uci_move}. "
            "Engine thinking..."
        )

        if not self.client.go():
            self.engine_thinking = False

            self.set_status(
                "Failed to start engine search."
            )

    # ========================================================
    # Game Over
    # ========================================================

    def check_game_over(self):
        if self.board.is_checkmate():
            self.game_over = True

            messagebox.showinfo(
                "Checkmate",
                "Checkmate! You win."
            )

            self.set_status(
                "Checkmate. You win!"
            )

            return True

        if self.board.is_stalemate():
            self.game_over = True

            messagebox.showinfo(
                "Stalemate",
                "Draw by stalemate."
            )

            self.set_status(
                "Stalemate."
            )

            return True

        return False

    # ========================================================
    # Promotion
    # ========================================================

    def choose_promotion(self):
        result = {
            "piece": None
        }

        dialog = tk.Toplevel(
            self.root
        )

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

    # ========================================================
    # Engine Bestmove
    # ========================================================

    def engine_bestmove(self, bestmove):
        # This function may be called by the networking thread.
        # Move the actual GUI operation onto Tkinter's thread.

        self.root.after(
            0,
            lambda: self.apply_engine_move(bestmove)
        )

    def apply_engine_move(self, bestmove):
        self.engine_thinking = False

        if self.game_over:
            return

        if bestmove in (
            "0000",
            "(none)",
            "none"
        ):
            self.game_over = True

            messagebox.showinfo(
                "Game Over",
                "The engine has no legal moves."
            )

            self.set_status(
                "Game over."
            )

            return

        # ----------------------------------------------------
        # Validate engine move before applying it.
        # ----------------------------------------------------

        if not self.board.is_legal_move(bestmove):
            self.game_over = True

            messagebox.showerror(
                "Engine Error",
                f"Engine returned illegal move: {bestmove}"
            )

            self.set_status(
                f"Engine returned illegal move: {bestmove}"
            )

            return

        # ----------------------------------------------------
        # Apply engine move.
        # ----------------------------------------------------

        if not self.board.make_uci_move(bestmove):
            self.game_over = True

            messagebox.showerror(
                "Engine Error",
                f"Failed to apply engine move: {bestmove}"
            )

            self.set_status(
                f"Failed to apply engine move: {bestmove}"
            )

            return

        self.draw_board()

        # ----------------------------------------------------
        # Check whether engine ended the game.
        # ----------------------------------------------------

        if self.board.is_checkmate():
            self.game_over = True

            messagebox.showinfo(
                "Checkmate",
                "Checkmate! The engine wins."
            )

            self.set_status(
                "Checkmate. Engine wins."
            )

            return

        if self.board.is_stalemate():
            self.game_over = True

            messagebox.showinfo(
                "Stalemate",
                "Draw by stalemate."
            )

            self.set_status(
                "Stalemate."
            )

            return

        self.set_status(
            f"Engine played {bestmove}. Your turn."
        )

    # ========================================================
    # Close
    # ========================================================

    def close(self):
        self.client.close()
        self.root.destroy()