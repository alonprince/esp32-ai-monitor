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


## Session 4: Report remaining quota percentage instead of used percentage

**Date**: 2026-07-17
**Task**: Report remaining quota percentage instead of used percentage
**Branch**: `feature/ui-fixes`

### Summary

Updated mac_host.py to calculate remaining percentage (100 - used_percentage) for Codex quota and return 100% on error.

### Main Changes

(Add details)

### Git Commits

| Hash | Message |
|------|---------|
| `8d4c795` | (see git log) |

### Testing

- [OK] (Add test results)

### Status

[OK] **Completed**

### Next Steps

- None - task complete


## Session 5: Convert Chinese text to Pinyin to prevent mojibake on Montserrat fonts

**Date**: 2026-07-17
**Task**: Convert Chinese text to Pinyin to prevent mojibake on Montserrat fonts
**Branch**: `feature/ui-fixes`

### Summary

Added clean_chinese_to_pinyin helper in mac_host.py which converts Chinese characters to Pinyin using pypinyin (falling back to stripping non-ASCII if pypinyin is not found) to prevent mojibake on ESP32 screen using ASCII-only Montserrat fonts.

### Main Changes

(Add details)

### Git Commits

| Hash | Message |
|------|---------|
| `74f29c2` | (see git log) |

### Testing

- [OK] (Add test results)

### Status

[OK] **Completed**

### Next Steps

- None - task complete


## Session 6: Implement battery level, native Chinese, and remove agy support

**Date**: 2026-07-18
**Task**: Implement battery level, native Chinese, and remove agy support
**Branch**: `feature/ui-fixes`

### Summary

Added Waveshare battery monitoring via AXP2101 PMU, native Chinese font configuration and partition enlargement, refined quota resets telemetry and atomically resolved telemetry race conditions and BLE writing thread safety, fully removed deprecated Antigravity agy support from codebase.

### Main Changes

(Add details)

### Git Commits

| Hash | Message |
|------|---------|
| `f337e49` | (see git log) |
| `908efbb` | (see git log) |
| `682d5ff` | (see git log) |
| `4953217` | (see git log) |
| `6ea7487` | (see git log) |

### Testing

- [OK] (Add test results)

### Status

[OK] **Completed**

### Next Steps

- None - task complete
