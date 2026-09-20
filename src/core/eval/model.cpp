#include "core/eval/model.hpp"

#include <stdexcept>


namespace
{

Ort::SessionOptions CreateSessionOptions()
{
    Ort::SessionOptions options;

    options.SetGraphOptimizationLevel(
        GraphOptimizationLevel::ORT_ENABLE_ALL
    );

    return options;
}


std::size_t GetPlane(Piece piece)
{
    if (piece == NULL_PIECE)
        return 0;

    const std::uint8_t type = get_piece_type(piece);
    const bool white = get_piece_color(piece) == PIECE_COLOR_WHITE;

    if (type < 1 || type > 6)
        throw std::runtime_error(
            "EvalModel: invalid piece type"
        );

    // Planes:
    //
    // 0  empty
    // 1  white pawn
    // 2  white knight
    // 3  white bishop
    // 4  white rook
    // 5  white queen
    // 6  white king
    // 7  black pawn
    // 8  black knight
    // 9  black bishop
    // 10 black rook
    // 11 black queen
    // 12 black king

    return 1 +
           (white ? 0 : 6) +
           (type - 1);
}

} // namespace


EvalModel::EvalModel(std::shared_ptr<ChessBoard> board)
    : env(
        ORT_LOGGING_LEVEL_WARNING,
        "Chess4"
    ),
      session(
          env,
          "data/eval.onnx",
          CreateSessionOptions()
      ),
      memory_info(
          Ort::MemoryInfo::CreateCpu(
              OrtArenaAllocator,
              OrtMemTypeDefault
          )
      ),
      board(std::move(board))
{
}


std::array<float, INPUT_SIZE> EvalModel::GetBoard() const
{
    std::array<float, INPUT_SIZE> output{};

    for (Square square = 0; square < 64; ++square)
    {
        const Piece piece =
            board->GetPieceAt(square);

        const std::size_t plane =
            GetPlane(piece);

        output[
            plane * 64 +
            static_cast<std::size_t>(square)
        ] = 1.0f;
    }

    return output;
}


Ort::Value EvalModel::CreateTensor(
    std::array<float, INPUT_SIZE>& data
)
{
    return Ort::Value::CreateTensor<float>(
        memory_info,
        data.data(),
        data.size(),
        INPUT_SHAPE.data(),
        INPUT_SHAPE.size()
    );
}


Evaluation EvalModel::Evaluate()
{
    auto input_data = GetBoard();

    auto input_tensor =
        CreateTensor(input_data);

    auto output_tensors = session.Run(
        Ort::RunOptions{nullptr},

        input_names,
        &input_tensor,
        1,

        output_names,
        1
    );

    if (output_tensors.empty())
    {
        throw std::runtime_error(
            "EvalModel: ONNX Runtime returned no output"
        );
    }

    const float* output =
        output_tensors[0].GetTensorData<float>();

    // Output in centipawns
    return static_cast<Evaluation>(output[0] * 100);
}
