# Repository history notes

## Date anomalies

Parts of this repository’s git history contain **author/committer dates in the future** relative to real calendar time (notably clusters in late 2026 and early 2027, e.g. synthetic `util(debug)` / `util(helper)` / `util(tool)` commit series). Those dates came from historical tooling that fabricated or bulk-dated commits, not from live work performed on those calendar days.

A full `git filter-branch` / history rewrite of hundreds of commits would damage authenticity signals and is **not** policy here. Anomalies are documented instead of rewritten.

Root commit example (ISO author date): see `git rev-list --max-parents=0 HEAD` and `git log -1 --format='%H %ad %s' --date=iso <hash>`.

## Policy going forward

- Prefer **chronological** commits with accurate author dates
- Do **not** reintroduce history-fabrication scripts (`expand_*.py`, `expand_project.py`, `expand_phase2.py`, `expand_commits.py`, `expand.py`)
- Prefer **feature + test** pairs in the same commit
- Do not force-push mass history rewrites to “fix” old dates

See also [CHANGELOG.md](../CHANGELOG.md) provenance notes and [CONTRIBUTING.md](../CONTRIBUTING.md).
