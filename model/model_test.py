import numpy as np
import onnxruntime as ort


# Load model
session = ort.InferenceSession(
    "../data/eval.onnx",
    providers=["CPUExecutionProvider"]
)


# Get input/output names
input_name = session.get_inputs()[0].name
output_name = session.get_outputs()[0].name


print("Input :", input_name)
print("Output:", output_name)


# Test inputs
X = np.array([
    [0.0, 0.0],
    [0.0, 1.0],
    [1.0, 0.0],
    [1.0, 1.0],
], dtype=np.float32)


# Run inference
outputs = session.run(
    [output_name],
    {
        input_name: X
    }
)


predictions = outputs[0]


for inputs, prediction in zip(X, predictions):
    print(
        f"{inputs} -> {prediction[0]:.6f}"
    )