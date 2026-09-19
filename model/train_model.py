import torch
import torch.nn as nn
import torch.optim as optim

from nnmodel import EvalNet


# ============================================================
# Configuration
# ============================================================

TRAINING_FILE = "training_positions.txt"

INPUT_SIZE = 13 * 64


# ============================================================
# Piece encoding
# ============================================================

# Each piece gets its own plane.
#
#   0  = empty
#   1  = white pawn
#   2  = white knight
#   3  = white bishop
#   4  = white rook
#   5  = white queen
#   6  = white king
#   7  = black pawn
#   8  = black knight
#   9  = black bishop
#   10 = black rook
#   11 = black queen
#   12 = black king


PIECE_TO_PLANE = {
    "P": 1,
    "N": 2,
    "B": 3,
    "R": 4,
    "Q": 5,
    "K": 6,

    "p": 7,
    "n": 8,
    "b": 9,
    "r": 10,
    "q": 11,
    "k": 12,
}


# ============================================================
# Load training data
# ============================================================

def load_positions(path):
    """
    Load:

        [<FEN> <eval>]

    into:

        X = neural-network inputs
        Y = Stockfish evaluations

    X shape:

        [number_of_positions, 832]

    Y shape:

        [number_of_positions, 1]
    """

    inputs = []
    evaluations = []

    with open(path, "r", encoding="utf-8") as file:
        for line_number, line in enumerate(file, start=1):
            line = line.strip()

            if not line:
                continue

            if not line.startswith("[") or not line.endswith("]"):
                print(
                    f"Warning: invalid line {line_number}: {line}"
                )
                continue

            # Remove [ and ].
            content = line[1:-1]

            fields = content.split()

            # FEN has six fields:
            #
            #   piece placement
            #   side to move
            #   castling
            #   en passant
            #   halfmove clock
            #   fullmove number
            #
            # followed by evaluation.
            if len(fields) != 7:
                print(
                    f"Warning: invalid line {line_number}: {line}"
                )
                continue

            fen = " ".join(fields[:6])

            try:
                evaluation = float(fields[6])
            except ValueError:
                print(
                    f"Warning: invalid evaluation "
                    f"on line {line_number}: {line}"
                )
                continue

            # ------------------------------------------------
            # FEN piece-placement field
            # ------------------------------------------------

            board = fen.split()[0]

            # 13 planes × 64 squares.
            encoded = [0.0] * INPUT_SIZE

            square = 0

            valid = True

            for character in board:

                if character == "/":
                    continue

                # A number represents that many empty squares.
                if character.isdigit():
                    square += int(character)
                    continue

                # Piece.
                plane = PIECE_TO_PLANE.get(character)

                if plane is None:
                    print(
                        f"Warning: unknown piece '{character}' "
                        f"on line {line_number}"
                    )
                    valid = False
                    break

                if square >= 64:
                    print(
                        f"Warning: too many squares "
                        f"on line {line_number}"
                    )
                    valid = False
                    break

                # ------------------------------------------------
                # Convert board square to Chess4 square indexing.
                #
                # FEN starts at A8 and goes down to A1.
                #
                # Chess4:
                #
                # A1 = 0
                # B1 = 1
                # ...
                # H1 = 7
                # A2 = 8
                # ...
                # H8 = 56
                #
                # FEN index:
                #
                # A8 = 0
                # B8 = 1
                # ...
                # A1 = 56
                #
                # Therefore:
                # ------------------------------------------------

                file_index = square % 8
                rank_from_top = square // 8

                rank_from_bottom = 7 - rank_from_top

                chess4_square = (
                    rank_from_bottom * 8
                    + file_index
                )

                # ------------------------------------------------
                # Store one-hot piece plane.
                # ------------------------------------------------

                index = (
                    plane * 64
                    + chess4_square
                )

                encoded[index] = 1.0

                square += 1

            if not valid:
                continue

            if square != 64:
                print(
                    f"Warning: invalid board on line "
                    f"{line_number}"
                )
                continue

            # Avoid mate positions. The search can find that on it's own.
            if evaluation < 900:
                inputs.append(encoded)
                evaluations.append([evaluation])

    X = torch.tensor(
        inputs,
        dtype=torch.float32,
    )

    Y = torch.tensor(
        evaluations,
        dtype=torch.float32,
    )

    return X, Y


# ============================================================
# Load dataset
# ============================================================

print(f"Loading training data: {TRAINING_FILE}")

X, Y = load_positions(TRAINING_FILE)

print(f"Positions: {len(X):,}")
print(f"Input size: {X.shape[1]}")
print(f"Expected: {INPUT_SIZE}")
print(f"Evaluation shape: {Y.shape}")

if len(X) == 0:
    raise RuntimeError(
        "No valid training positions were loaded."
    )


# ============================================================
# Create model
# ============================================================

model = EvalNet()

print()
print(model)


# ============================================================
# Training
# ============================================================

# Stockfish evaluation is a continuous value in pawns,
# so this is a regression problem rather than classification.

loss_function = nn.MSELoss()

optimizer = optim.Adam(
    model.parameters(),
    lr=0.001,
)


EPOCHS = 20000


for epoch in range(EPOCHS):

    # Forward pass.
    prediction = model(X)

    # Calculate error.
    loss = loss_function(
        prediction,
        Y,
    )

    # Clear old gradients.
    optimizer.zero_grad()

    # Calculate gradients.
    loss.backward()

    # Update weights.
    optimizer.step()

    if epoch % 100 == 0:
        print(
            f"Epoch {epoch:4d} "
            f"Loss {loss.item():.6f}"
        )


# ============================================================
# Test the trained model
# ============================================================

print("\nTraining results:")

model.eval()

with torch.no_grad():

    predictions = model(X)

    # Show the first 10 positions.
    count = min(10, len(X))

    for i in range(count):
        actual = Y[i].item()
        predicted = predictions[i].item()

        print(
            f"Stockfish: {actual:+8.3f} "
            f"Network: {predicted:+8.3f} "
            f"Error: {predicted - actual:+8.3f}"
        )


# ============================================================
# Export to ONNX
# ============================================================

torch.onnx.export(
    model,
    torch.randn(1, 832),
    "model/eval.onnx",
    input_names=["input"],
    output_names=["output"],
    dynamo=True
)


print("\nSaved eval.onnx")
