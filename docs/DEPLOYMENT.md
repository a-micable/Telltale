# Deployment

Telltale is a **local CLI / library**. There is **no production deployment** path in this repository.

## What this project ships

- Build with `make` or CMake (`cmake -S . -B build-cmake`)
- Run the `./telltale` binary on the host
- Test with `make test` (canonical) or `pytest -q` / CTest wrappers

## What does **not** exist (by design)

| Artifact | Status |
|----------|--------|
| Production cloud deploy | **None** |
| Infrastructure-as-code (Terraform, Pulumi, CloudFormation) | **None** |
| Kubernetes / Helm / Ansible | **None** |
| Dockerfile / docker-compose / `.devcontainer` | **Removed** — not part of the product |

Do not add container or IaC packaging to “deploy” Telltale. Distribute and run it as a locally built C++17 binary only.

See also [PROJECT_CLASSIFICATION.md](PROJECT_CLASSIFICATION.md) and [ARCHITECTURE.md](ARCHITECTURE.md).
