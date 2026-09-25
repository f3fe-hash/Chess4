FROM gcc:15-trixie AS engine-builder

RUN apt-get update \
    && apt-get install --no-install-recommends -y \
        python3-pip \
        libncurses-dev \
        libonnx-dev \
        libonnxruntime-dev \
        libprotobuf-dev \
        protobuf-compiler \
    && python3 -m pip install --break-system-packages --no-cache-dir cmake \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

COPY . .

# Verify all PGO/model files exist.
RUN test -f /src/games.fen \
    && test -f /src/data/eval.onnx \
    && test -f /src/data/eval.onnx.data

# ============================================================
# PGO generation
# ============================================================

RUN cmake -S . -B /build \
        -DCHESS_UCI_SERVER=OFF \
        -DBUILD_CHESS_TESTS=OFF \
        -DCMAKE_BUILD_TYPE=Release \
        -DPGO_MODE=GENERATE \
    && cmake --build /build --target Chess --parallel

# games.fen + eval.onnx + eval.onnx.data are available here.
RUN cd /src && /build/Chess

# ============================================================
# PGO optimized build
# ============================================================

RUN cmake -S . -B /build \
        -DCHESS_UCI_SERVER=ON \
        -DBUILD_CHESS_TESTS=OFF \
        -DCMAKE_BUILD_TYPE=Release \
        -DPGO_MODE=USE \
    && cmake --build /build --target Chess --parallel

# ============================================================
# Runtime
# ============================================================

FROM gcc:15-trixie

ENV PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1

WORKDIR /app

COPY requirements.txt .

RUN apt-get update \
    && apt-get install --no-install-recommends -y \
        python3-pip \
        libncurses6 \
        libonnxruntime1.21 \
    && rm -rf /var/lib/apt/lists/* \
    && python3 -m pip install --break-system-packages --no-cache-dir -r requirements.txt

COPY --from=engine-builder /build/Chess ./Chess

# ONNX model + external tensor data.
COPY data/eval.onnx ./data/eval.onnx
COPY data/eval.onnx.data ./data/eval.onnx.data

COPY lichess ./lichess

EXPOSE 8080

CMD ["./Chess"]