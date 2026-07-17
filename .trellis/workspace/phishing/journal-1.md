# Journal - phishing (Part 1)

> AI development session journal
> Started: 2026-07-14

---



## Session 1: Event-driven task tracking and database token quota query

**Date**: 2026-07-17
**Task**: Event-driven task tracking and database token quota query
**Branch**: `feature/ui-fixes`

### Summary

Replaced file watcher and subprocess loops with an event-driven hook socket listener and direct SQLite query of ~/.codex/state_5.sqlite for real-time task and quota tracking.

### Main Changes

(Add details)

### Git Commits

| Hash | Message |
|------|---------|
| `ba35d84` | (see git log) |

### Testing

- [OK] (Add test results)

### Status

[OK] **Completed**

### Next Steps

- None - task complete


## Session 2: Format active tool string to map Task Name and ID on ESP32

**Date**: 2026-07-17
**Task**: Format active tool string to map Task Name and ID on ESP32
**Branch**: `feature/ui-fixes`

### Summary

Updated handle_json_command and auto_codex_db_loop to format the active tool as a CSV payload (name,id,time,status) so that the ESP32 screen can parse and render the correct Task ID and Task Name (using the user prompt) when Codex runs.

### Main Changes

(Add details)

### Git Commits

| Hash | Message |
|------|---------|
| `f3e60d3` | (see git log) |

### Testing

- [OK] (Add test results)

### Status

[OK] **Completed**

### Next Steps

- None - task complete


## Session 3: Calculate and send overall account quota percentage instead of session tokens

**Date**: 2026-07-17
**Task**: Calculate and send overall account quota percentage instead of session tokens
**Branch**: `feature/ui-fixes`

### Summary

Modified mac_host.py to parse opencode stats output and compute the overall billing/token quota percentage based on the configured limit (cost-based for budgets <=1000, token-based for limits >1000). Updates are triggered instantly when hook events or SQLite thread states change.

### Main Changes

(Add details)

### Git Commits

| Hash | Message |
|------|---------|
| `b2d44e3` | (see git log) |

### Testing

- [OK] (Add test results)

### Status

[OK] **Completed**

### Next Steps

- None - task complete
