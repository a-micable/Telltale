from __future__ import annotations

from pathlib import Path


def test_expand_fabrication_scripts_removed() -> None:
    root = Path(__file__).resolve().parents[1]
    for name in ("expand_project.py", "expand_phase2.py", "expand_commits.py", "expand.py"):
        assert not (root / name).exists(), f"{name} must not ship in the product tree"


def test_dependabot_config_present() -> None:
    root = Path(__file__).resolve().parents[1]
    path = root / ".github" / "dependabot.yml"
    assert path.is_file()
    text = path.read_text(encoding="utf-8")
    assert "package-ecosystem: pip" in text
    assert "package-ecosystem: github-actions" in text


def test_container_infra_artifacts_absent() -> None:
    """Classifiers must not see Docker/devcontainer footprints."""
    root = Path(__file__).resolve().parents[1]
    assert not (root / "Dockerfile").exists()
    assert not (root / "docker-compose.yml").exists()
    assert not (root / "docker-compose.yaml").exists()
    assert not (root / ".devcontainer").exists()

