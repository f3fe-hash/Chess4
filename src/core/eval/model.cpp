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

    // We have a tiny NN. Use 1 thread for the work,
    // as multiple threads create too much overhead.
    options.SetIntraOpNumThreads(1);
    options.SetInterOpNumThreads(1);

    return options;
}


std::size_t GetPlane(Piece piece)
{
    if (piece == NULL_PIECE)
        return 0;

    const std::uint8_t type =
        get_piece_type(piece);

    if (type < 1 || type > 6)
        throw std::runtime_error(
            "EvalModel: invalid piece type"
        );

    // Piece types are already:
    //
    // 1 = pawn
    // 2 = knight
    // 3 = bishop
    // 4 = rook
    // 5 = queen
    // 6 = king
    //
    // Convert to zero-based plane indexing.

    return type - 1;
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

        if (piece == NULL_PIECE)
            continue;

        const std::size_t plane =
            GetPlane(piece);

        const bool white =
            get_piece_color(piece) ==
            PIECE_COLOR_WHITE;

        const float value =
            white ? 1.0f : -1.0f;

        output[
            plane * 64 +
            static_cast<std::size_t>(square)
        ] = value;
    }

    return output;
}


Ort::Value EvalModel::CreateTensor(const std::array<float, INPUT_SIZE>& data) const
{
    return Ort::Value::CreateTensor<float>(
        memory_info,
        const_cast<float*>(data.data()),
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
