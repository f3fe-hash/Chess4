#include "core/eval/model.hpp"
#include <onnx/onnx_pb.h>

#include <fstream>
#include <stdexcept>
#include <string>


namespace
{

constexpr const char* LAYER1_WEIGHT =
    "network.0.weight";

constexpr const char* LAYER1_BIAS =
    "network.0.bias";

constexpr const char* LAYER2_WEIGHT =
    "network.2.weight";

constexpr const char* LAYER2_BIAS =
    "network.2.bias";

constexpr const char* LAYER3_WEIGHT =
    "network.4.weight";

constexpr const char* LAYER3_BIAS =
    "network.4.bias";

} // namespace


std::size_t EvalModel::GetPlane(Piece piece)
{
    if (piece == NULL_PIECE)
        return 0;

    const std::uint8_t type =
        get_piece_type(piece);

    if (type < 1 || type > 6)
    {
        throw std::runtime_error(
            "EvalModel: invalid piece type"
        );
    }

    return type - 1;
}


float EvalModel::LeakyReLU(
    const float value,
    const float slope
)
{
    return value > 0.0F
        ? value
        : value * slope;
}


const onnx::TensorProto& FindInitializer(
    const onnx::GraphProto& graph,
    const char* name
)
{
    for (const auto& initializer : graph.initializer())
    {
        if (initializer.name() == name)
            return initializer;
    }

    throw std::runtime_error(
        std::string("EvalModel: ONNX initializer not found: ") +
        name
    );
}


void CopyTensor(
    const onnx::TensorProto& tensor,
    float* destination,
    const std::size_t size
)
{
    if (tensor.data_type() != onnx::TensorProto::FLOAT)
    {
        throw std::runtime_error(
            "EvalModel: expected FLOAT tensor"
        );
    }

    std::size_t tensor_size = 1;

    for (const auto dimension : tensor.dims())
    {
        tensor_size *=
            static_cast<std::size_t>(dimension);
    }

    if (tensor_size != size)
    {
        throw std::runtime_error(
            "EvalModel: unexpected ONNX tensor size"
        );
    }


    // --------------------------------------------------------
    // Embedded raw data
    // --------------------------------------------------------

    if (!tensor.raw_data().empty())
    {
        if (tensor.raw_data().size() !=
            size * sizeof(float))
        {
            throw std::runtime_error(
                "EvalModel: invalid raw tensor size"
            );
        }

        std::memcpy(
            destination,
            tensor.raw_data().data(),
            size * sizeof(float)
        );

        return;
    }


    // --------------------------------------------------------
    // Legacy float_data
    // --------------------------------------------------------

    if (tensor.float_data_size() != 0)
    {
        if (static_cast<std::size_t>(
                tensor.float_data_size()
            ) != size)
        {
            throw std::runtime_error(
                "EvalModel: invalid float tensor size"
            );
        }

        for (std::size_t i = 0; i < size; ++i)
        {
            destination[i] =
                tensor.float_data(
                    static_cast<int>(i)
                );
        }

        return;
    }


    // --------------------------------------------------------
    // External data
    // --------------------------------------------------------

    if (tensor.external_data_size() != 0)
    {
        std::string location;

        std::size_t offset = 0;
        std::size_t length =
            size * sizeof(float);

        for (const auto& entry : tensor.external_data())
        {
            if (entry.key() == "location")
            {
                location = entry.value();
            }
            else if (entry.key() == "offset")
            {
                offset =
                    std::stoull(entry.value());
            }
            else if (entry.key() == "length")
            {
                length =
                    std::stoull(entry.value());
            }
        }

        if (location.empty())
        {
            throw std::runtime_error(
                "EvalModel: external tensor has no location"
            );
        }

        if (length != size * sizeof(float))
        {
            throw std::runtime_error(
                "EvalModel: invalid external tensor size"
            );
        }


        // eval.onnx.data is relative to eval.onnx.
        std::ifstream file(
            "data/" + location,
            std::ios::binary
        );

        if (!file)
        {
            throw std::runtime_error(
                "EvalModel: unable to open external tensor data: " +
                location
            );
        }


        file.seekg(
            static_cast<std::streamoff>(offset),
            std::ios::beg
        );

        if (!file)
        {
            throw std::runtime_error(
                "EvalModel: unable to seek external tensor data"
            );
        }


        file.read(
            reinterpret_cast<char*>(destination),
            static_cast<std::streamsize>(length)
        );

        if (file.gcount() !=
            static_cast<std::streamsize>(length))
        {
            throw std::runtime_error(
                "EvalModel: unable to read external tensor data"
            );
        }

        return;
    }


    throw std::runtime_error(
        "EvalModel: tensor contains no float data"
    );
}


void EvalModel::LoadModel()
{
    std::ifstream file(
        MODEL_PATH,
        std::ios::binary
    );

    if (!file)
    {
        throw std::runtime_error(
            "EvalModel: unable to open " +
            std::string(MODEL_PATH)
        );
    }


    onnx::ModelProto model;

    if (!model.ParseFromIstream(&file))
    {
        throw std::runtime_error(
            "EvalModel: unable to parse ONNX model"
        );
    }


    const onnx::GraphProto& graph =
        model.graph();


    const onnx::TensorProto& layer1_weight =
        FindInitializer(
            graph,
            LAYER1_WEIGHT
        );

    const onnx::TensorProto& layer1_bias =
        FindInitializer(
            graph,
            LAYER1_BIAS
        );

    const onnx::TensorProto& layer2_weight =
        FindInitializer(
            graph,
            LAYER2_WEIGHT
        );

    const onnx::TensorProto& layer2_bias =
        FindInitializer(
            graph,
            LAYER2_BIAS
        );

    const onnx::TensorProto& layer3_weight =
        FindInitializer(
            graph,
            LAYER3_WEIGHT
        );

    const onnx::TensorProto& layer3_bias =
        FindInitializer(
            graph,
            LAYER3_BIAS
        );


    CopyTensor(
        layer1_weight,
        weights.layer1.data(),
        weights.layer1.size()
    );

    CopyTensor(
        layer1_bias,
        weights.bias1.data(),
        weights.bias1.size()
    );

    CopyTensor(
        layer2_weight,
        weights.layer2.data(),
        weights.layer2.size()
    );

    CopyTensor(
        layer2_bias,
        weights.bias2.data(),
        weights.bias2.size()
    );

    CopyTensor(
        layer3_weight,
        weights.layer3.data(),
        weights.layer3.size()
    );

    CopyTensor(
        layer3_bias,
        weights.bias3.data(),
        weights.bias3.size()
    );
}


EvalModel::EvalModel(
    std::shared_ptr<ChessBoard> board
)
    : board(std::move(board))
{
    LoadModel();
}


std::array<float, INPUT_SIZE>
EvalModel::GetBoard() const
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
            white ? 1.0F : -1.0F;

        output[
            plane * 64 +
            static_cast<std::size_t>(square)
        ] = value;
    }

    return output;
}


float EvalModel::Forward(
    const std::array<float, INPUT_SIZE>& input
) const
{
    Hidden1Array hidden1{};

    // --------------------------------------------------------
    // Linear 1: 384 -> 8
    //
    // PyTorch:
    //     nn.Linear(384, 8)
    //
    // Weight shape:
    //     [8][384]
    // --------------------------------------------------------

    for (std::size_t output = 0;
         output < HIDDEN1_SIZE;
         ++output)
    {
        float sum =
            weights.bias1[output];

        for (std::size_t input_index = 0;
             input_index < INPUT_SIZE;
             ++input_index)
        {
            sum +=
                weights.layer1[
                    output * INPUT_SIZE +
                    input_index
                ] *
                input[input_index];
        }

        hidden1[output] =
            LeakyReLU(
                sum,
                LEAKY_RELU_1
            );
    }


    Hidden2Array hidden2{};

    // --------------------------------------------------------
    // Linear 2: 8 -> 4
    // --------------------------------------------------------

    for (std::size_t output = 0;
         output < HIDDEN2_SIZE;
         ++output)
    {
        float sum =
            weights.bias2[output];

        for (std::size_t input_index = 0;
             input_index < HIDDEN1_SIZE;
             ++input_index)
        {
            sum +=
                weights.layer2[
                    output * HIDDEN1_SIZE +
                    input_index
                ] *
                hidden1[input_index];
        }

        hidden2[output] =
            LeakyReLU(
                sum,
                LEAKY_RELU_2
            );
    }


    // --------------------------------------------------------
    // Linear 3: 4 -> 1
    // --------------------------------------------------------

    float output =
        weights.bias3[0];

    for (std::size_t input_index = 0;
         input_index < HIDDEN2_SIZE;
         ++input_index)
    {
        output +=
            weights.layer3[input_index] *
            hidden2[input_index];
    }


    return output;
}


void EvalModel::Store(
    const ZobristHash& key,
    const Evaluation eval
)
{
    Bucket& bucket =
        buckets[key % NUM_BUCKETS];

    if (bucket.num_entries >= EVAL_ENTRIES)
        return;

    EvalEntry& entry =
        bucket.entries[bucket.num_entries++];

    entry.key = key;
    entry.eval = eval;
}


Evaluation EvalModel::Get(
    const ZobristHash& key,
    bool& found
)
{
    const Bucket& bucket =
        buckets[key % NUM_BUCKETS];

    for (std::size_t i = 0;
         i < bucket.num_entries;
         ++i)
    {
        const EvalEntry& entry =
            bucket.entries[i];

        if (entry.key == key)
        {
            found = true;
            return entry.eval;
        }
    }

    found = false;
    return 0;
}


Evaluation EvalModel::Evaluate()
{
    // First, see if it is cached.
    const ZobristHash key =
        board->GetZobristHash();

    bool found = false;

    const Evaluation cached =
        Get(key, found);

    if (found)
        return cached;


    const auto input =
        GetBoard();


    // ONNX Runtime is NOT involved here.
    //
    // Forward() performs:
    //
    // 384 -> 8 -> LeakyReLU
    //      -> 4 -> LeakyReLU
    //      -> 1
    //
    const float output =
        Forward(input);


    const Evaluation cp =
        static_cast<Evaluation>(
            output * 100.0F
        );


    Store(key, cp);

    return cp;
}