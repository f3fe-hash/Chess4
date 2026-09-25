#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

#include <onnx/onnx_pb.h>

#include "chess.hpp"
#include "core/scoring.hpp"

constexpr std::size_t INPUT_SIZE = 6 * 64;

constexpr std::size_t HIDDEN1_SIZE = 8;
constexpr std::size_t HIDDEN2_SIZE = 4;
constexpr std::size_t OUTPUT_SIZE = 1;

constexpr std::size_t EVAL_ENTRIES = 16;
constexpr std::size_t NUM_BUCKETS = 131072;


class EvalModel
{
private:
    using InputArray =
        std::array<float, INPUT_SIZE>;

    using Hidden1Array =
        std::array<float, HIDDEN1_SIZE>;

    using Hidden2Array =
        std::array<float, HIDDEN2_SIZE>;

    using OutputArray =
        std::array<float, OUTPUT_SIZE>;


    struct Weights
    {
        std::array<float, HIDDEN1_SIZE * INPUT_SIZE>
            layer1{};

        std::array<float, HIDDEN1_SIZE>
            bias1{};

        std::array<float, HIDDEN2_SIZE * HIDDEN1_SIZE>
            layer2{};

        std::array<float, HIDDEN2_SIZE>
            bias2{};

        std::array<float, OUTPUT_SIZE * HIDDEN2_SIZE>
            layer3{};

        std::array<float, OUTPUT_SIZE>
            bias3{};
    };


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


    std::shared_ptr<ChessBoard> board;

    Weights weights{};

    Bucket buckets[NUM_BUCKETS];


    static constexpr float LEAKY_RELU_1 = 0.6F;
    static constexpr float LEAKY_RELU_2 = 0.4F;


    static constexpr const char* MODEL_PATH =
        "data/eval.onnx";


    static std::size_t GetPlane(Piece piece);

    static float LeakyReLU(
        float value,
        float slope
    );


    void LoadModel();

    static const onnx::TensorProto& FindInitializer(
        const onnx::GraphProto& graph,
        const char* name
    );

    static void CopyTensor(
        const onnx::TensorProto& tensor,
        float* destination,
        std::size_t size
    );


    std::array<float, INPUT_SIZE> GetBoard() const;

    float Forward(
        const std::array<float, INPUT_SIZE>& input
    ) const;


    void Store(
        const ZobristHash& key,
        const Evaluation eval
    );

    Evaluation Get(
        const ZobristHash& key,
        bool& found
    );


public:
    explicit EvalModel(
        std::shared_ptr<ChessBoard> board
    );

    ~EvalModel() = default;

    Evaluation Evaluate();
};

