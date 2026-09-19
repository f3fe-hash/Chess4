#pragma once

#include <array>
#include <cstdint>
#include <memory>

#include <onnxruntime_cxx_api.h>

#include "chess.hpp"
#include "core/scoring.hpp"

constexpr std::size_t INPUT_SIZE = 13 * 64;

class EvalModel
{
private:
    Ort::Env env;
    Ort::Session session;
    Ort::MemoryInfo memory_info;

    std::shared_ptr<ChessBoard> board;

    static constexpr std::array<int64_t, 2> INPUT_SHAPE = {
        1,
        static_cast<int64_t>(INPUT_SIZE)
    };

    const char* input_names[1] = {"input"};
    const char* output_names[1] = {"output"};

    std::array<float, INPUT_SIZE> GetBoard() const;

    Ort::Value CreateTensor(
        std::array<float, INPUT_SIZE>& data
    );

public:
    explicit EvalModel(std::shared_ptr<ChessBoard> board);

    ~EvalModel() = default;

    Evaluation Evaluate();
};
