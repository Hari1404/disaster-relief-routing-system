FROM gcc:13 AS build
WORKDIR /app
COPY . .
# Link libstdc++ and libgcc statically: the runtime image below ships an older libstdc++ than the
# gcc:13 build image, and a dynamic link fails at start with "GLIBCXX_3.4.32 not found".
RUN g++ -std=c++17 -O2 -I engine -I server engine/*.cpp server/Api.cpp server/main.cpp -o relief_server -pthread -static-libstdc++ -static-libgcc

FROM debian:bookworm-slim
WORKDIR /app
COPY --from=build /app/relief_server .
COPY data ./data
COPY web ./web
ENV BIND_HOST=0.0.0.0
# Set APP_USER and APP_PASS in the hosting dashboard. The server refuses to start without them.
CMD ["./relief_server"]
