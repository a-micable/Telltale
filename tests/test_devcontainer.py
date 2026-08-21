from __future__ import annotations

from pathlib import Path


def test_devcontainer_json_exists_and_mentions_cpp() -> None:
    root = Path(__file__).resolve().parents[1]
    path = root / ".devcontainer" / "devcontainer.json"
    assert path.is_file(), ".devcontainer/devcontainer.json must exist"
    text = path.read_text(encoding="utf-8")
    assert "cpp" in text.lower()
