FROM gcc:15-bookworm AS engine-builder

RUN apt-get update \
    && apt-get install --no-install-recommends -y python3-pip libncurses-dev \
    && python3 -m pip install --break-system-packages --no-cache-dir cmake \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN cmake -S . -B /build \
        -DCHESS_UCI_SERVER=ON \
        -DBUILD_CHESS_TESTS=OFF \
        -DCMAKE_BUILD_TYPE=Release \
    && cmake --build /build --target Chess --parallel

FROM gcc:15-bookworm

ENV PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1

WORKDIR /app

COPY requirements.txt .
RUN apt-get update \
    && apt-get install --no-install-recommends -y python3-pip libncurses6 \
    && rm -rf /var/lib/apt/lists/* \
    && python3 -m pip install --break-system-packages --no-cache-dir -r requirements.txt

COPY --from=engine-builder /build/Chess ./Chess
COPY lichess ./lichess

EXPOSE 8080
