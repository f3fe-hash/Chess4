import tkinter as tk
from tkinter import messagebox
from pathlib import Path
from time import monotonic

from board import ChessBoard
from config import (
    BOARD_SIZE,
    SQUARE_SIZE,
    LIGHT_SQUARE,
    DARK_SQUARE,
    SELECT_SQUARE,
    USE_UCICODE_PIECES,
    COORDINATE_FONT,
    DEFAULT_TIME_CONTROL,
    TIME_CONTROLS,
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
        self.root.geometry("900x700")
        self.root.minsize(700, 620)
        self.root.resizable(True, True)

        self.board_size = BOARD_SIZE
        self.square_size = SQUARE_SIZE

        # ----------------------------------------------------
        # Game state
        # ----------------------------------------------------

        self.board = ChessBoard()

        self.dragging = False
        self.drag_piece = None
        self.drag_from = None

        self.engine_thinking = False
        self.game_over = False
        self.ignore_next_bestmove = False
        self.closing = False
        self.clock_after_id = None
        self.connect_after_id = None

        self.time_control = DEFAULT_TIME_CONTROL
        self.white_time = 0.0
        self.black_time = 0.0
        self.increment = 0
        self.clock_started = False
        self.last_clock_update = monotonic()

        self.piece_images = {}
        if not USE_UCICODE_PIECES:
            asset_root = Path(__file__).parent
            for piece, filename in PIECES_TO_FILE.items():
                self.piece_images[piece] = tk.PhotoImage(
                    file=str(asset_root / filename)
                ).subsample(2, 2)

        self.root.configure(bg="#20252b")

        self.root.grid_rowconfigure(0, weight=1)
        self.root.grid_columnconfigure(0, weight=1)
        self.root.grid_columnconfigure(1, weight=0)

        self.left_frame = tk.Frame(root, bg="#20252b")
        self.left_frame.grid(row=0, column=0, sticky="nsew", padx=(15, 5), pady=10)
        self.left_frame.grid_rowconfigure(1, weight=1)
        self.left_frame.grid_columnconfigure(0, weight=1)

        self.right_frame = tk.Frame(root, bg="#20252b", width=230)
        self.right_frame.grid(row=0, column=1, sticky="ns", padx=(5, 10), pady=10)
        self.right_frame.grid_propagate(False)

        # ----------------------------------------------------
        # Match header
        # ----------------------------------------------------

        self.header = tk.Frame(self.left_frame, bg="#20252b")
        self.header.grid(row=0, column=0, sticky="ew", pady=(0, 5))

        self.black_clock_var = tk.StringVar()
        self.white_clock_var = tk.StringVar()

        self.clock_frame = tk.Frame(self.header, bg="#20252b")
        self.clock_frame.pack()

        tk.Label(
            self.clock_frame, text="BLACK", font=("DejaVu Sans", 9, "bold"),
            fg="#aab3bd", bg="#20252b"
        ).pack(side="left", padx=(0, 6))

        self.black_clock_label = tk.Label(
            self.clock_frame, textvariable=self.black_clock_var,
            font=("DejaVu Sans", 20, "bold"), fg="#f4f1ea",
            bg="#20252b", width=7, anchor="w"
        )
        self.black_clock_label.pack(side="left")

        tk.Label(
            self.clock_frame, text="WHITE", font=("DejaVu Sans", 9, "bold"),
            fg="#aab3bd", bg="#20252b"
        ).pack(side="left", padx=(16, 6))

        self.white_clock_label = tk.Label(
            self.clock_frame, textvariable=self.white_clock_var,
            font=("DejaVu Sans", 20, "bold"), fg="#f4f1ea",
            bg="#20252b", width=7, anchor="w"
        )
        self.white_clock_label.pack(side="left")

        # ----------------------------------------------------
        # Board
        # ----------------------------------------------------

        self.board_frame = tk.Frame(self.left_frame, bg="#20252b")
        self.board_frame.grid(row=1, column=0, sticky="nsew")

        self.canvas = tk.Canvas(
            self.board_frame,
            width=BOARD_SIZE,
            height=BOARD_SIZE,
            highlightthickness=0
        )

        self.canvas.pack(
            pady=(10, 5),
            expand=True,
            anchor="w"
        )

        self.board_frame.bind("<Configure>", self.on_layout_resize)

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
            self.right_frame,
            textvariable=self.status_var,
            anchor="w", fg="#d6dbe0", bg="#20252b"
        )

        self.status_label.pack(
            fill="x",
            pady=(20, 20)
        )

        # ----------------------------------------------------
        # Controls
        # ----------------------------------------------------

        self.control_frame = tk.Frame(self.right_frame, bg="#20252b")

        self.control_frame.pack(
            fill="x",
            pady=(0, 10)
        )

        self.new_game_button = tk.Button(
            self.control_frame,
            text="New game",
            command=self.new_game,
            bg="#d8a84e", activebackground="#edc36e",
            relief="flat", padx=10
        )

        self.new_game_button.pack(
            fill="x"
        )

        self.time_control_var = tk.StringVar(value=self.time_control)
        self.time_control_menu = tk.OptionMenu(
            self.control_frame, self.time_control_var, *TIME_CONTROLS,
            command=self.change_time_control
        )
        self.time_control_menu.config(
            bg="#3a424b", fg="#f4f1ea", activebackground="#56616d",
            activeforeground="#ffffff", relief="flat", highlightthickness=0
        )
        self.time_control_menu.pack(fill="x", pady=6)

        self.reconnect_button = tk.Button(
            self.control_frame, text="Reconnect", command=self.reconnect,
            bg="#3a424b", fg="#f4f1ea", activebackground="#56616d",
            relief="flat", padx=10
        )
        self.reconnect_button.pack(fill="x")

        self.resign_button = tk.Button(
            self.control_frame, text="Resign", command=self.resign,
            bg="#8f4141", fg="#ffffff", activebackground="#b95858",
            relief="flat", padx=10
        )
        self.resign_button.pack(fill="x", pady=(20, 0))

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
        self.change_time_control(self.time_control)
        self.clock_after_id = self.root.after(200, self.update_clock)

        # Connect after Tkinter starts.
        self.connect_after_id = self.root.after(
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

    def reconnect(self):
        self.client.close()
        self.set_status("Reconnecting...")
        self.root.after(100, self.connect)

    def change_time_control(self, name):
        self.time_control = name
        white_seconds, black_seconds, self.increment = TIME_CONTROLS[name]
        self.white_time = float(white_seconds)
        self.black_time = float(black_seconds)
        self.clock_started = False
        self.last_clock_update = monotonic()
        self.update_clock_labels()

    def update_clock_labels(self):
        self.white_clock_var.set(self.format_time(self.white_time))
        self.black_clock_var.set(self.format_time(self.black_time))

    @staticmethod
    def format_time(seconds):
        seconds = max(0, int(seconds))
        return f"{seconds // 60:02d}:{seconds % 60:02d}"

    def update_clock(self):
        if self.closing:
            return

        now = monotonic()
        elapsed = now - self.last_clock_update
        self.last_clock_update = now

        if self.clock_started and not self.game_over:
            if self.board.turn == "w":
                self.white_time -= elapsed
            else:
                self.black_time -= elapsed

            if self.white_time <= 0 or self.black_time <= 0:
                self.white_time = max(0, self.white_time)
                self.black_time = max(0, self.black_time)
                self.game_over = True
                self.engine_thinking = False
                winner = "Black" if self.white_time == 0 else "White"
                self.set_status(f"Time expired. {winner} wins.")
                messagebox.showinfo("Time", f"Time expired. {winner} wins.")

        self.update_clock_labels()
        self.clock_after_id = self.root.after(200, self.update_clock)

    # ========================================================
    # Status
    # ========================================================

    def set_status(self, text):
        if self.closing:
            return

        self.root.after(
            0,
            lambda: self.status_var.set(text)
            if not self.closing else None
        )

    # ========================================================
    # New Game
    # ========================================================

    def new_game(self):
        if not self.client.connected:
            messagebox.showerror(
                "Connection Error",
                "The UCI server is not connected."
            )
            return

        if self.engine_thinking:
            self.ignore_next_bestmove = True
            self.client.stop()

        self.board.reset()

        self.dragging = False
        self.drag_piece = None
        self.drag_from = None

        self.engine_thinking = False
        self.game_over = False
        self.change_time_control(self.time_control)

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

    def resign(self):
        if self.game_over:
            return

        if self.engine_thinking:
            self.ignore_next_bestmove = True
            self.client.stop()
            self.engine_thinking = False

        self.game_over = True
        self.set_status("You resigned. Engine wins.")
        messagebox.showinfo("Resignation", "You resigned. Engine wins.")

    # ========================================================
    # Drawing
    # ========================================================

    def on_layout_resize(self, event):
        available_height = event.height - 15
        available_size = min(event.width, available_height)
        board_size = max(1, available_size)

        if board_size == self.board_size:
            return

        self.board_size = board_size
        self.square_size = board_size / 8
        self.canvas.configure(width=board_size, height=board_size)
        self.draw_board()

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
                x1 = col * self.square_size
                y1 = row * self.square_size

                x2 = x1 + self.square_size
                y2 = y1 + self.square_size

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
                            x1 + self.square_size / 2,
                            y1 + self.square_size / 2,
                            text=UNICODE_PIECES[piece], # type: ignore
                            font=PIECE_FONT, # type: ignore
                            fill="black"
                        )
                    else:
                        self.canvas.create_image(
                            x1 + self.square_size / 2,
                            y1 + self.square_size / 2,
                            image=self.piece_images[piece]
                        )

        self.draw_coordinates()

    def draw_coordinates(self):
        for col in range(8):
            file = chr(ord("a") + col)

            self.canvas.create_text(
                col * self.square_size + 5,
                self.board_size - 5,
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
                row * self.square_size + 5,
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
            0 <= x < self.board_size
            and 0 <= y < self.board_size
        ):
            return None

        col = int(x // self.square_size)
        row = int(y // self.square_size)

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
                image=self.piece_images[self.drag_piece]
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

        self.white_time += self.increment
        self.clock_started = True

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

        engine_time = min(1000, int(self.black_time * 1000))
        if not self.client.go(engine_time):
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

        if self.ignore_next_bestmove:
            self.ignore_next_bestmove = False
            return

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

        self.black_time += self.increment
        self.clock_started = True

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
        if self.closing:
            return

        self.closing = True

        for after_id in (self.clock_after_id, self.connect_after_id):
            if after_id is not None:
                try:
                    self.root.after_cancel(after_id)
                except tk.TclError:
                    pass

        self.client.close()
        self.root.quit()
        self.root.destroy()