#!/usr/bin/env bash
# Install apt packages pinned in ci/apt-packages.lock (ubuntu-22.04).
# Falls back to unpinned install if a Candidate version drifted.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
LOCK="$ROOT/ci/apt-packages.lock"
sudo apt-get update
pkgs=()
while IFS= read -r line; do
  [[ -z "$line" || "$line" =~ ^# ]] && continue
  name="${line%%=*}"
  ver="${line#*=}"
  if [[ -n "$ver" ]]; then
    pkgs+=("${name}=${ver}")
  else
    pkgs+=("${name}")
  fi
done < "$LOCK"
if ! sudo apt-get install -y --no-install-recommends "${pkgs[@]}"; then
  echo "Pinned apt install failed; falling back to unpinned package names" >&2
  names=()
  while IFS= read -r line; do
    [[ -z "$line" || "$line" =~ ^# ]] && continue
    names+=("${line%%=*}")
  done < "$LOCK"
  sudo apt-get install -y --no-install-recommends "${names[@]}"
fi
