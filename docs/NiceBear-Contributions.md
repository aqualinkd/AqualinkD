# NiceBear Contribution and Engineering Guide

## Principles

- Work in small, verifiable increments.
- Instrument before changing production behavior.
- Test with safe existing values first.
- Distinguish API acceptance from hardware success.
- Require controller read-back verification.
- Remove temporary diagnostics after the behavior is understood.
- Preserve verified milestones with commits and annotated tags.

## Recommended workflow

```text
Inspect
  |
Form hypothesis
  |
Instrument
  |
Build and deploy
  |
Run one controlled test
  |
Capture evidence
  |
Implement production behavior
  |
Remove temporary diagnostics
  |
Document
  |
Commit and tag
```

## Git checks

```bash
git status --short
git diff --check
git diff --stat
git diff --cached --check
git diff --cached --stat
```

## Build checks

```bash
git restore release/
rm -rf build
make -j"$(nproc)"
```

## Upstream pull requests

Document the capability, controller behavior, implementation approach, hardware
tested, known limitations, and future extension points. Do not present
single-system validation as universal compatibility.
