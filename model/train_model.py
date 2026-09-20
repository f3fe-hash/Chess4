#!/usr/bin/env python3

import argparse
from pathlib import Path

import torch
import torch.nn as nn
import torch.optim as optim

from nnmodel import EvalNet


# ============================================================
# Configuration
# ============================================================

INPUT_SIZE = 13 * 64


# ============================================================
# Piece encoding
# ============================================================

# Each piece gets its own plane.
#
#   0  = unused / empty plane
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
#
# The input therefore contains 13 * 64 values.
#
# Plane 0 is intentionally unused, matching the existing
# encoding scheme.

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

    with open(
        path,
        "r",
        encoding="utf-8",
    ) as file:

        for line_number, line in enumerate(
            file,
            start=1,
        ):
            line = line.strip()

            if not line:
                continue

            if (
                not line.startswith("[")
                or not line.endswith("]")
            ):
                print(
                    f"Warning: invalid line "
                    f"{line_number}: {line}"
                )
                continue

            content = line[1:-1]

            fields = content.split()

            # FEN contains six fields followed by
            # the evaluation.
            if len(fields) != 7:
                print(
                    f"Warning: invalid line "
                    f"{line_number}: {line}"
                )
                continue

            fen = " ".join(
                fields[:6]
            )

            try:
                evaluation = float(
                    fields[6]
                )

            except ValueError:
                print(
                    f"Warning: invalid evaluation "
                    f"on line {line_number}: {line}"
                )
                continue

            # ------------------------------------------------
            # FEN piece-placement field.
            # ------------------------------------------------

            board = fen.split()[0]

            # 13 planes × 64 squares.
            encoded = [0.0] * INPUT_SIZE

            square = 0

            valid = True

            for character in board:

                if character == "/":
                    continue

                # A digit represents that many empty squares.
                if character.isdigit():
                    square += int(character)
                    continue

                plane = PIECE_TO_PLANE.get(
                    character
                )

                if plane is None:
                    print(
                        f"Warning: unknown piece "
                        f"'{character}' on line "
                        f"{line_number}"
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
                # Convert FEN square indexing to Chess4 indexing.
                #
                # FEN:
                #
                #   A8 ... H8
                #   A7 ... H7
                #   ...
                #   A1 ... H1
                #
                # Chess4:
                #
                #   A1 = 0
                #   B1 = 1
                #   ...
                #   H1 = 7
                #   A2 = 8
                #   ...
                #   H8 = 63
                # ------------------------------------------------

                file_index = square % 8

                rank_from_top = square // 8

                rank_from_bottom = (
                    7 - rank_from_top
                )

                chess4_square = (
                    rank_from_bottom * 8
                    + file_index
                )

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

            # Avoid mate positions.
            #
            # The Stockfish generator represents mate as
            # approximately +/-1000.
            if abs(evaluation) >= 900.0:
                continue

            inputs.append(encoded)
            evaluations.append(
                [evaluation]
            )

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
# Training
# ============================================================

def train_model(
    X,
    Y,
    epochs,
    learning_rate,
    device,
):
    """
    Train EvalNet as a regression model.
    """

    model = EvalNet().to(device)

    print()
    print(model)

    print()
    print(
        f"Training device: {device}"
    )

    print(
        f"Epochs:          {epochs:,}"
    )

    print(
        f"Learning rate:   {learning_rate}"
    )

    # Stockfish evaluation is a continuous value in pawns,
    # so this is a regression problem.
    loss_function = nn.MSELoss()

    optimizer = optim.Adam(
        model.parameters(),
        lr=learning_rate,
    )

    X = X.to(device)
    Y = Y.to(device)

    model.train()

    for epoch in range(epochs):

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

        if (
            epoch == 0
            or epoch % 100 == 0
            or epoch == epochs - 1
        ):
            print(
                f"Epoch {epoch:6d} "
                f"Loss {loss.item():.6f}",
                flush=True,
            )

    return model


# ============================================================
# Test model
# ============================================================

def test_model(
    model,
    X,
    Y,
    device,
):
    """
    Print predictions for the first ten positions.
    """

    print()
    print(
        "Training results:"
    )

    model.eval()

    X = X.to(device)
    Y = Y.to(device)

    with torch.no_grad():
        predictions = model(X)

    count = min(
        10,
        len(X),
    )

    for i in range(count):
        actual = Y[i].item()
        predicted = predictions[i].item()

        print(
            f"Stockfish: {actual:+8.3f} "
            f"Network: {predicted:+8.3f} "
            f"Error: {predicted - actual:+8.3f}"
        )


# ============================================================
# ONNX export
# ============================================================

def export_onnx(
    model,
    output_path,
    device,
):
    """
    Export EvalNet to ONNX.
    """

    output_path.parent.mkdir(
        parents=True,
        exist_ok=True,
    )

    model.eval()

    dummy_input = torch.randn(
        1,
        INPUT_SIZE,
        device=device,
    )

    print()
    print(
        f"Exporting ONNX model to: "
        f"{output_path}"
    )

    torch.onnx.export(
        model,
        dummy_input,
        str(output_path),
        input_names=["input"],
        output_names=["output"],
        dynamo=True,
    )

    print(
        f"Saved: {output_path}"
    )


# ============================================================
# Main
# ============================================================

def main():
    parser = argparse.ArgumentParser(
        description=(
            "Train Chess4 EvalNet from Stockfish "
            "training positions."
        )
    )

    parser.add_argument(
        "--training-file",
        type=Path,
        default=Path(
            "./data/training_positions.txt"
        ),
        help="Training-position file",
    )

    parser.add_argument(
        "--output",
        type=Path,
        default=Path(
            "/app/model/eval.onnx"
        ),
        help="Output ONNX model",
    )

    parser.add_argument(
        "--epochs",
        type=int,
        default=20000,
        help="Number of training epochs",
    )

    parser.add_argument(
        "--learning-rate",
        type=float,
        default=0.001,
        help="Adam learning rate",
    )

    parser.add_argument(
        "--device",
        default="cpu",
        choices=["cpu"],
        help="Training device",
    )

    args = parser.parse_args()

    # --------------------------------------------------------
    # Validate.
    # --------------------------------------------------------

    if not args.training_file.exists():
        raise FileNotFoundError(
            f"Training file does not exist: "
            f"{args.training_file}"
        )

    if args.epochs <= 0:
        raise ValueError(
            "--epochs must be greater than zero"
        )

    if args.learning_rate <= 0:
        raise ValueError(
            "--learning-rate must be greater than zero"
        )

    # --------------------------------------------------------
    # Device.
    # --------------------------------------------------------

    device = torch.device(
        args.device
    )

    print(
        f"Using device: {device}"
    )

    # --------------------------------------------------------
    # Load dataset.
    # --------------------------------------------------------

    print()
    print(
        f"Loading training data: "
        f"{args.training_file}"
    )

    X, Y = load_positions(
        args.training_file
    )

    print(
        f"Positions:       {len(X):,}"
    )

    if len(X) == 0:
        raise RuntimeError(
            "No valid training positions were loaded."
        )

    if X.ndim != 2:
        raise RuntimeError(
            f"Unexpected input dimensions: "
            f"{X.shape}"
        )

    if X.shape[1] != INPUT_SIZE:
        raise RuntimeError(
            f"Unexpected input size: "
            f"{X.shape[1]} "
            f"(expected {INPUT_SIZE})"
        )

    print(
        f"Input size:      {X.shape[1]}"
    )

    print(
        f"Expected:        {INPUT_SIZE}"
    )

    print(
        f"Evaluation shape: {Y.shape}"
    )

    # --------------------------------------------------------
    # Train.
    # --------------------------------------------------------

    model = train_model(
        X,
        Y,
        args.epochs,
        args.learning_rate,
        device,
    )

    # --------------------------------------------------------
    # Test.
    # --------------------------------------------------------

    test_model(
        model,
        X,
        Y,
        device,
    )

    # --------------------------------------------------------
    # Export.
    # --------------------------------------------------------

    export_onnx(
        model,
        args.output,
        device,
    )

    print()
    print(
        "Training complete."
    )

    return 0


if __name__ == "__main__":
    raise SystemExit(
        main()
    )
