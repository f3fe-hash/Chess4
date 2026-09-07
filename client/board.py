class ChessBoard:
    def __init__(self):
        self.reset()

    def reset(self):
        self.board = [
            ["r", "n", "b", "q", "k", "b", "n", "r"],
            ["p", "p", "p", "p", "p", "p", "p", "p"],
            [None, None, None, None, None, None, None, None],
            [None, None, None, None, None, None, None, None],
            [None, None, None, None, None, None, None, None],
            [None, None, None, None, None, None, None, None],
            ["P", "P", "P", "P", "P", "P", "P", "P"],
            ["R", "N", "B", "Q", "K", "B", "N", "R"],
        ]

        self.turn = "w"

        self.castling = {
            "K": True,
            "Q": True,
            "k": True,
            "q": True,
        }

        self.en_passant = None
        self.halfmove_clock = 0
        self.fullmove_number = 1
        self.move_history = []

    def load_fen(self, fen):
        fields = fen.strip().split()

        if len(fields) not in (4, 5, 6):
            raise ValueError("FEN must contain 4, 5, or 6 fields")

        ranks = fields[0].split("/")
        if len(ranks) != 8:
            raise ValueError("FEN piece placement must contain 8 ranks")

        board = []
        for rank in ranks:
            row = []
            for character in rank:
                if character in "12345678":
                    row.extend([None] * int(character))
                elif character in "PNBRQKpnbrqk":
                    row.append(character)
                else:
                    raise ValueError(f"Invalid FEN piece: {character}")

            if len(row) != 8:
                raise ValueError("Each FEN rank must contain 8 squares")
            board.append(row)

        if fields[1] not in ("w", "b"):
            raise ValueError("FEN active color must be 'w' or 'b'")

        castling = {right: False for right in "KQkq"}
        if fields[2] != "-":
            if any(right not in castling for right in fields[2]):
                raise ValueError("Invalid FEN castling rights")
            for right in fields[2]:
                castling[right] = True

        en_passant = fields[3]
        if en_passant != "-":
            if (
                len(en_passant) != 2
                or en_passant[0] not in "abcdefgh"
                or en_passant[1] not in "36"
            ):
                raise ValueError("Invalid FEN en-passant square")
        else:
            en_passant = None

        halfmove_clock = 0
        fullmove_number = 1
        if len(fields) >= 5:
            try:
                halfmove_clock = int(fields[4])
            except ValueError as error:
                raise ValueError("Invalid FEN halfmove clock") from error
            if halfmove_clock < 0:
                raise ValueError("Invalid FEN halfmove clock")

        if len(fields) == 6:
            try:
                fullmove_number = int(fields[5])
            except ValueError as error:
                raise ValueError("Invalid FEN fullmove number") from error
            if fullmove_number < 1:
                raise ValueError("Invalid FEN fullmove number")

        self.board = board
        self.turn = fields[1]
        self.castling = castling
        self.en_passant = en_passant
        self.halfmove_clock = halfmove_clock
        self.fullmove_number = fullmove_number
        self.move_history = []

    def to_fen(self):
        placement = []
        for row in self.board:
            empty = 0
            rank = []
            for piece in row:
                if piece is None:
                    empty += 1
                else:
                    if empty:
                        rank.append(str(empty))
                        empty = 0
                    rank.append(piece)
            if empty:
                rank.append(str(empty))
            placement.append("".join(rank))

        castling = "".join(
            right for right in "KQkq" if self.castling[right]
        ) or "-"

        return " ".join((
            "/".join(placement),
            self.turn,
            castling,
            self.en_passant or "-",
            str(self.halfmove_clock),
            str(self.fullmove_number),
        ))

    # ========================================================
    # Coordinate conversion
    # ========================================================

    @staticmethod
    def square_to_coords(square):
        file = ord(square[0]) - ord("a")
        rank = int(square[1])

        return 8 - rank, file

    @staticmethod
    def coords_to_square(row, col):
        return chr(ord("a") + col) + str(8 - row)

    # ========================================================
    # Piece color
    # ========================================================

    @staticmethod
    def piece_color(piece):
        if piece is None:
            return None

        return "w" if piece.isupper() else "b"

    # ========================================================
    # Board helpers
    # ========================================================

    @staticmethod
    def opposite_color(color):
        return "b" if color == "w" else "w"

    def find_king(self, color):
        king = "K" if color == "w" else "k"

        for row in range(8):
            for col in range(8):
                if self.board[row][col] == king:
                    return row, col

        return None

    # ========================================================
    # Is square attacked?
    # ========================================================

    def is_square_attacked(self, row, col, by_color):
        """
        Return True if (row, col) is attacked by by_color.
        """

        # ----------------------------------------------------
        # Pawns
        # ----------------------------------------------------

        if by_color == "w":
            pawn = "P"

            # White pawns attack one row upward.
            pawn_row = row + 1

            if pawn_row < 8:
                if col - 1 >= 0:
                    if self.board[pawn_row][col - 1] == pawn:
                        return True

                if col + 1 < 8:
                    if self.board[pawn_row][col + 1] == pawn:
                        return True

        else:
            pawn = "p"

            # Black pawns attack one row downward.
            pawn_row = row - 1

            if pawn_row >= 0:
                if col - 1 >= 0:
                    if self.board[pawn_row][col - 1] == pawn:
                        return True

                if col + 1 < 8:
                    if self.board[pawn_row][col + 1] == pawn:
                        return True

        # ----------------------------------------------------
        # Knights
        # ----------------------------------------------------

        knight = "N" if by_color == "w" else "n"

        knight_offsets = [
            (-2, -1),
            (-2,  1),
            (-1, -2),
            (-1,  2),
            (1, -2),
            (1,  2),
            (2, -1),
            (2,  1),
        ]

        for dr, dc in knight_offsets:
            r = row + dr
            c = col + dc

            if 0 <= r < 8 and 0 <= c < 8:
                if self.board[r][c] == knight:
                    return True

        # ----------------------------------------------------
        # Kings
        # ----------------------------------------------------

        king = "K" if by_color == "w" else "k"

        for dr in (-1, 0, 1):
            for dc in (-1, 0, 1):
                if dr == 0 and dc == 0:
                    continue

                r = row + dr
                c = col + dc

                if 0 <= r < 8 and 0 <= c < 8:
                    if self.board[r][c] == king:
                        return True

        # ----------------------------------------------------
        # Rooks / Queens
        # ----------------------------------------------------

        rook = "R" if by_color == "w" else "r"
        queen = "Q" if by_color == "w" else "q"

        rook_directions = [
            (-1, 0),
            (1, 0),
            (0, -1),
            (0, 1),
        ]

        for dr, dc in rook_directions:
            r = row + dr
            c = col + dc

            while 0 <= r < 8 and 0 <= c < 8:
                piece = self.board[r][c]

                if piece is not None:
                    if piece == rook or piece == queen:
                        return True

                    break

                r += dr
                c += dc

        # ----------------------------------------------------
        # Bishops / Queens
        # ----------------------------------------------------

        bishop = "B" if by_color == "w" else "b"

        bishop_directions = [
            (-1, -1),
            (-1, 1),
            (1, -1),
            (1, 1),
        ]

        for dr, dc in bishop_directions:
            r = row + dr
            c = col + dc

            while 0 <= r < 8 and 0 <= c < 8:
                piece = self.board[r][c]

                if piece is not None:
                    if piece == bishop or piece == queen:
                        return True

                    break

                r += dr
                c += dc

        return False

    # ========================================================
    # Check
    # ========================================================

    def is_in_check(self, color):
        king_position = self.find_king(color)

        # Invalid board position.
        if king_position is None:
            return True

        king_row, king_col = king_position

        return self.is_square_attacked(
            king_row,
            king_col,
            self.opposite_color(color)
        )

    # ========================================================
    # Make move internally
    #
    # This does NOT check legality.
    # ========================================================

    def _make_uci_move_unchecked(self, uci_move):
        from_square = uci_move[0:2]
        to_square = uci_move[2:4]

        promotion = None

        if len(uci_move) >= 5:
            promotion = uci_move[4].lower()

        from_row, from_col = self.square_to_coords(from_square)
        to_row, to_col = self.square_to_coords(to_square)

        piece = self.board[from_row][from_col]

        if piece is None:
            return False

        captured = self.board[to_row][to_col]
        moving_color = self.turn

        # ----------------------------------------------------
        # En passant
        # ----------------------------------------------------

        if (
            piece.lower() == "p"
            and from_col != to_col
            and captured is None
        ):
            captured_row = from_row

            captured_piece = self.board[captured_row][to_col]

            if (
                captured_piece is not None
                and captured_piece.lower() == "p"
            ):
                self.board[captured_row][to_col] = None
                captured = captured_piece

        # ----------------------------------------------------
        # Move piece
        # ----------------------------------------------------

        self.board[from_row][from_col] = None
        self.board[to_row][to_col] = piece

        # ----------------------------------------------------
        # Promotion
        # ----------------------------------------------------

        if promotion is not None and piece.lower() == "p":
            if piece.isupper():
                self.board[to_row][to_col] = promotion.upper()
            else:
                self.board[to_row][to_col] = promotion.lower()

        # ----------------------------------------------------
        # Castling
        # ----------------------------------------------------

        if piece.lower() == "k":

            # White king-side.
            if from_square == "e1" and to_square == "g1":
                self.board[7][7] = None
                self.board[7][5] = "R"

            # White queen-side.
            elif from_square == "e1" and to_square == "c1":
                self.board[7][0] = None
                self.board[7][3] = "R"

            # Black king-side.
            elif from_square == "e8" and to_square == "g8":
                self.board[0][7] = None
                self.board[0][5] = "r"

            # Black queen-side.
            elif from_square == "e8" and to_square == "c8":
                self.board[0][0] = None
                self.board[0][3] = "r"

        # ----------------------------------------------------
        # Castling rights
        # ----------------------------------------------------

        if piece == "K":
            self.castling["K"] = False
            self.castling["Q"] = False

        elif piece == "k":
            self.castling["k"] = False
            self.castling["q"] = False

        elif piece == "R":
            if from_square == "h1":
                self.castling["K"] = False
            elif from_square == "a1":
                self.castling["Q"] = False

        elif piece == "r":
            if from_square == "h8":
                self.castling["k"] = False
            elif from_square == "a8":
                self.castling["q"] = False

        # Captured rook.
        if captured == "R":
            if to_square == "h1":
                self.castling["K"] = False
            elif to_square == "a1":
                self.castling["Q"] = False

        elif captured == "r":
            if to_square == "h8":
                self.castling["k"] = False
            elif to_square == "a8":
                self.castling["q"] = False

        # ----------------------------------------------------
        # En-passant target
        # ----------------------------------------------------

        self.en_passant = None

        if piece.lower() == "p":
            if abs(to_row - from_row) == 2:
                ep_row = (from_row + to_row) // 2

                self.en_passant = self.coords_to_square(
                    ep_row,
                    from_col
                )

        # ----------------------------------------------------
        # Turn
        # ----------------------------------------------------

        if piece.lower() == "p" or captured is not None:
            self.halfmove_clock = 0
        else:
            self.halfmove_clock += 1

        if moving_color == "b":
            self.fullmove_number += 1

        self.turn = self.opposite_color(self.turn)

        self.move_history.append(uci_move)

        return True

    # ========================================================
    # Make legal UCI move
    # ========================================================

    def make_uci_move(self, uci_move):
        if not self.is_legal_move(uci_move):
            return False

        return self._make_uci_move_unchecked(uci_move)

    # ========================================================
    # Generate pseudo-legal moves
    # ========================================================

    def _generate_pseudo_legal_moves(self):
        color = self.turn

        for row in range(8):
            for col in range(8):
                piece = self.board[row][col]

                if piece is None:
                    continue

                if self.piece_color(piece) != color:
                    continue

                piece_type = piece.lower()

                from_square = self.coords_to_square(
                    row,
                    col
                )

                # ------------------------------------------------
                # Pawn
                # ------------------------------------------------

                if piece_type == "p":
                    direction = -1 if color == "w" else 1
                    start_row = 6 if color == "w" else 1
                    promotion_row = 0 if color == "w" else 7

                    # One square forward.
                    next_row = row + direction

                    if 0 <= next_row < 8:
                        if self.board[next_row][col] is None:

                            to_square = self.coords_to_square(
                                next_row,
                                col
                            )

                            if next_row == promotion_row:
                                for promotion in "qrbn":
                                    yield (
                                        from_square +
                                        to_square +
                                        promotion
                                    )
                            else:
                                yield from_square + to_square

                            # Two squares forward.
                            if row == start_row:
                                two_row = row + direction * 2

                                if (
                                    self.board[two_row][col]
                                    is None
                                ):
                                    yield (
                                        from_square +
                                        self.coords_to_square(
                                            two_row,
                                            col
                                        )
                                    )

                    # Pawn captures.
                    for dc in (-1, 1):
                        capture_col = col + dc
                        capture_row = row + direction

                        if not (
                            0 <= capture_row < 8
                            and 0 <= capture_col < 8
                        ):
                            continue

                        target = self.board[
                            capture_row
                        ][capture_col]

                        target_square = self.coords_to_square(
                            capture_row,
                            capture_col
                        )

                        # Normal capture.
                        if (
                            target is not None
                            and self.piece_color(target) != color
                        ):
                            if capture_row == promotion_row:
                                for promotion in "qrbn":
                                    yield (
                                        from_square +
                                        target_square +
                                        promotion
                                    )
                            else:
                                yield (
                                    from_square +
                                    target_square
                                )

                        # En passant.
                        elif target is None:
                            if (
                                self.en_passant
                                == target_square
                            ):
                                yield (
                                    from_square +
                                    target_square
                                )

                # ------------------------------------------------
                # Knight
                # ------------------------------------------------

                elif piece_type == "n":
                    offsets = [
                        (-2, -1),
                        (-2, 1),
                        (-1, -2),
                        (-1, 2),
                        (1, -2),
                        (1, 2),
                        (2, -1),
                        (2, 1),
                    ]

                    for dr, dc in offsets:
                        r = row + dr
                        c = col + dc

                        if not (0 <= r < 8 and 0 <= c < 8):
                            continue

                        target = self.board[r][c]

                        if (
                            target is None
                            or self.piece_color(target) != color
                        ):
                            yield (
                                from_square +
                                self.coords_to_square(r, c)
                            )

                # ------------------------------------------------
                # Bishop / Rook / Queen
                # ------------------------------------------------

                elif piece_type in ("b", "r", "q"):

                    directions = []

                    if piece_type in ("b", "q"):
                        directions += [
                            (-1, -1),
                            (-1, 1),
                            (1, -1),
                            (1, 1),
                        ]

                    if piece_type in ("r", "q"):
                        directions += [
                            (-1, 0),
                            (1, 0),
                            (0, -1),
                            (0, 1),
                        ]

                    for dr, dc in directions:
                        r = row + dr
                        c = col + dc

                        while 0 <= r < 8 and 0 <= c < 8:
                            target = self.board[r][c]

                            if target is None:
                                yield (
                                    from_square +
                                    self.coords_to_square(r, c)
                                )

                            else:
                                if (
                                    self.piece_color(target)
                                    != color
                                ):
                                    yield (
                                        from_square +
                                        self.coords_to_square(r, c)
                                    )

                                break

                            r += dr
                            c += dc

                # ------------------------------------------------
                # King
                # ------------------------------------------------

                elif piece_type == "k":
                    for dr in (-1, 0, 1):
                        for dc in (-1, 0, 1):
                            if dr == 0 and dc == 0:
                                continue

                            r = row + dr
                            c = col + dc

                            if not (
                                0 <= r < 8
                                and 0 <= c < 8
                            ):
                                continue

                            target = self.board[r][c]

                            if (
                                target is None
                                or self.piece_color(target) != color
                            ):
                                yield (
                                    from_square +
                                    self.coords_to_square(r, c)
                                )

                    # ------------------------------------------------
                    # Castling
                    # ------------------------------------------------

                    if color == "w" and row == 7 and col == 4:

                        # King-side.
                        if (
                            self.castling["K"]
                            and self.board[7][5] is None
                            and self.board[7][6] is None
                            and self.board[7][7] == "R"
                        ):
                            yield "e1g1"

                        # Queen-side.
                        if (
                            self.castling["Q"]
                            and self.board[7][1] is None
                            and self.board[7][2] is None
                            and self.board[7][3] is None
                            and self.board[7][0] == "R"
                        ):
                            yield "e1c1"

                    elif color == "b" and row == 0 and col == 4:

                        # King-side.
                        if (
                            self.castling["k"]
                            and self.board[0][5] is None
                            and self.board[0][6] is None
                            and self.board[0][7] == "r"
                        ):
                            yield "e8g8"

                        # Queen-side.
                        if (
                            self.castling["q"]
                            and self.board[0][1] is None
                            and self.board[0][2] is None
                            and self.board[0][3] is None
                            and self.board[0][0] == "r"
                        ):
                            yield "e8c8"

    # ========================================================
    # Legal move test
    # ========================================================

    def is_legal_move(self, uci_move):
        if len(uci_move) not in (4, 5):
            return False

        from_square = uci_move[0:2]
        to_square = uci_move[2:4]

        # Basic square validation.
        if (
            len(from_square) != 2
            or len(to_square) != 2
            or from_square[0] not in "abcdefgh"
            or to_square[0] not in "abcdefgh"
            or from_square[1] not in "12345678"
            or to_square[1] not in "12345678"
        ):
            return False

        from_row, from_col = self.square_to_coords(from_square)
        to_row, to_col = self.square_to_coords(to_square)

        piece = self.board[from_row][from_col]

        if piece is None:
            return False

        # Must be our turn.
        if self.piece_color(piece) != self.turn:
            return False

        # Can't capture our own piece.
        destination = self.board[to_row][to_col]

        if (
            destination is not None
            and self.piece_color(destination) == self.turn
        ):
            return False

        # ----------------------------------------------------
        # Promotion validation.
        # ----------------------------------------------------

        if len(uci_move) == 5:
            if piece.lower() != "p":
                return False

            promotion = uci_move[4].lower()

            if promotion not in "qrbn":
                return False

            if to_row not in (0, 7):
                return False

        # ----------------------------------------------------
        # Must be a pseudo-legal move.
        # ----------------------------------------------------

        if uci_move not in self._generate_pseudo_legal_moves():
            return False

        # ----------------------------------------------------
        # Special castling rule:
        #
        # The king may not castle while in check or through
        # an attacked square.
        # ----------------------------------------------------

        if piece.lower() == "k":
            if (
                from_square == "e1"
                and to_square == "g1"
            ):
                if self.is_in_check("w"):
                    return False

                if self.is_square_attacked(
                    7, 5, "b"
                ):
                    return False

            elif (
                from_square == "e1"
                and to_square == "c1"
            ):
                if self.is_in_check("w"):
                    return False

                if self.is_square_attacked(
                    7, 3, "b"
                ):
                    return False

            elif (
                from_square == "e8"
                and to_square == "g8"
            ):
                if self.is_in_check("b"):
                    return False

                if self.is_square_attacked(
                    0, 5, "w"
                ):
                    return False

            elif (
                from_square == "e8"
                and to_square == "c8"
            ):
                if self.is_in_check("b"):
                    return False

                if self.is_square_attacked(
                    0, 3, "w"
                ):
                    return False

        # ----------------------------------------------------
        # Make a temporary copy and see whether our king
        # remains safe.
        # ----------------------------------------------------

        import copy

        test_board = copy.deepcopy(self)

        test_board._make_uci_move_unchecked(uci_move)

        # Our own king cannot be left in check.
        if test_board.is_in_check(self.turn):
            return False

        return True

    # ========================================================
    # Legal move generation
    # ========================================================

    def legal_moves(self):
        """
        Return all legal moves in UCI notation.
        """

        moves = []

        for move in self._generate_pseudo_legal_moves():
            if self.is_legal_move(move):
                moves.append(move)

        return moves

    # ========================================================
    # Checkmate / stalemate
    # ========================================================

    def is_checkmate(self):
        return (
            self.is_in_check(self.turn)
            and len(self.legal_moves()) == 0
        )

    def is_stalemate(self):
        return (
            not self.is_in_check(self.turn)
            and len(self.legal_moves()) == 0
        )

    def is_game_over(self):
        return (
            self.is_checkmate()
            or self.is_stalemate()
        )
