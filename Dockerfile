# Self-contained build and runtime for telltale (no external services).
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends build-essential \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY include/ include/
COPY src/ src/
COPY tests/test_common.hpp tests/test_common.hpp
COPY tests/test_common.cpp tests/test_common.cpp
COPY tests/test_binary_io.cpp tests/test_binary_io.cpp
COPY tests/test_dispatcher.cpp tests/test_dispatcher.cpp
COPY tests/test_filter_engine.cpp tests/test_filter_engine.cpp
COPY tests/test_text_format.cpp tests/test_text_format.cpp
COPY tests/test_telltale.cpp tests/test_telltale.cpp
COPY Makefile .

RUN make && make test

# Default: show CLI help. Override with write/replay/etc.
ENTRYPOINT ["./telltale"]
CMD ["help"]
