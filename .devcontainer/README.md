# Dev container

Open this repository in a [dev container](https://containers.dev/) (VS Code / Cursor: “Reopen in Container”).

The image is `mcr.microsoft.com/devcontainers/cpp` (Ubuntu 22.04). On create it installs CMake and runs `make && make test`.

This is a **C++ CLI/library** workspace — not an infrastructure/IaC project. See [`PROJECT_TYPE`](../PROJECT_TYPE) (`cli-tool`) and [`PROJECT_CLASSIFICATION.md`](PROJECT_CLASSIFICATION.md).
