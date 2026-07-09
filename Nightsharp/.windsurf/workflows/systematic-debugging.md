---
description: Systematic debugging - find root cause before fixing bugs
---

Invoke the `systematic-debugging` skill, then follow its process:

1. **Reproduce** the bug reliably. Define exact steps + expected vs actual behavior.
2. **Isolate** — narrow down with binary search (git bisect, comment-out halves, minimal repro).
3. **Root cause** — trace backward from symptom to origin. Use `root-cause-tracing.md` from the skill.
4. **Fix at root** — patch the source, not the symptom. Minimal change.
5. **Verify** — confirm fix resolves repro AND run regression tests.
6. **Defense in depth** — add guard/assert/test to prevent recurrence (see `defense-in-depth.md`).

Do NOT propose fixes before completing steps 1-3.
