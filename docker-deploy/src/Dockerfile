FROM ubuntu:22.04

RUN apt-get update && \
    apt-get install -y g++ make && \
    apt-get clean

WORKDIR /app

COPY src ./src

RUN mkdir -p /var/log/erss

RUN g++ -std=c++17 \
    src/proxy/server.cpp \
    src/proxy/utils.cpp \
    src/cache/cache.cpp \
    src/cache/get_pipeline.cpp \
    src/log/logger.cpp \
    -o proxy

EXPOSE 12345

CMD ["./proxy"]