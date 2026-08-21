from __future__ import annotations

from pathlib import Path


def test_env_example_documents_log_level() -> None:
    root = Path(__file__).resolve().parents[1]
    text = (root / ".env.example").read_text(encoding="utf-8")
    assert "TELLTALE_LOG_LEVEL" in text


def test_logging_hpp_names_framework() -> None:
    root = Path(__file__).resolve().parents[1]
    header = (root / "include" / "telltale" / "logging.hpp").read_text(encoding="utf-8")
    assert "LOGGING_FRAMEWORK" in header
    assert "telltale_structured_logger" in header
    assert "configure_from_env" in header
    assert "TELLTALE_LOG_LEVEL" in header
