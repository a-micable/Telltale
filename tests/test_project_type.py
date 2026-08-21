from __future__ import annotations

from pathlib import Path


def test_project_type_is_cli_tool() -> None:
    root = Path(__file__).resolve().parents[1]
    text = (root / "PROJECT_TYPE").read_text(encoding="utf-8").strip()
    assert text == "cli-tool"


def test_classification_doc_rejects_iac() -> None:
    root = Path(__file__).resolve().parents[1]
    doc = (root / "docs" / "PROJECT_CLASSIFICATION.md").read_text(encoding="utf-8")
    assert "CLI" in doc or "cli" in doc
    assert "Terraform" in doc
    assert "not" in doc.lower()


def test_classification_lists_removed_containers() -> None:
    root = Path(__file__).resolve().parents[1]
    doc = (root / "docs" / "PROJECT_CLASSIFICATION.md").read_text(encoding="utf-8")
    assert "Dockerfile" in doc
    assert "removed" in doc.lower() or "not exist" in doc.lower()

