FROM gcc:13 AS build
WORKDIR /app
COPY . .
RUN g++ -std=c++17 -O2 -I engine -I server engine/*.cpp server/Api.cpp server/main.cpp -o relief_server -pthread

FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y --no-install-recommends libstdc++6 && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY --from=build /app/relief_server .
COPY data ./data
COPY web ./web
ENV BIND_HOST=0.0.0.0
# Set APP_USER and APP_PASS in the hosting dashboard. The server refuses to start without them.
CMD ["./relief_server"]
