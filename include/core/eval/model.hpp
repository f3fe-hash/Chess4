#pragma once

#include <array>
#include <cstdint>
#include <memory>

#include <onnxruntime_cxx_api.h>

#include "chess.hpp"
#include "core/scoring.hpp"

constexpr std::size_t INPUT_SIZE = 6 * 64;

constexpr std::size_t EVAL_ENTRIES = 16;
constexpr std::size_t NUM_BUCKETS = 131072;

class EvalModel
{
private:
    Ort::Env env;
    Ort::Session session;
    Ort::MemoryInfo memory_info;

    std::shared_ptr<ChessBoard> board;

    struct EvalEntry
    {
        ZobristHash key = 0;
        Evaluation eval = 0;
    };

    struct Bucket
    {
        EvalEntry entries[EVAL_ENTRIES];
        std::size_t num_entries = 0;
    };

    Bucket buckets[NUM_BUCKETS];

    static constexpr std::array<int64_t, 2> INPUT_SHAPE = {
        1,
        static_cast<int64_t>(INPUT_SIZE)
    };

    static constexpr const char* input_names[1] = {"input"};
    static constexpr const char* output_names[1] = {"output"};

    std::array<float, INPUT_SIZE> GetBoard() const;

    Ort::Value CreateTensor(const std::array<float, INPUT_SIZE>& data) const;

    void Store(const ZobristHash& key, const Evaluation eval);
    Evaluation Get(const ZobristHash& key, bool& found);

public:
    explicit EvalModel(std::shared_ptr<ChessBoard> board);

    ~EvalModel() = default;

    Evaluation Evaluate();
};
