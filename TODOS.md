# TODOS

## UI / Display Hardware

## Completed

### Slanted Screen Distortion Fix

**What:** Lower QSPI clock frequency to 20MHz and enforce 4-byte QSPI DMA alignment on redraw area width.

**Why:** Fixes diagonal/slanted italicized screen distortion on the AMOLED panel.

**Context:** The C display driver was previously running at 40MHz causing reflections, and non-aligned buffer width sizes caused slanted display outputs.

**Effort:** S
**Priority:** P0

**Completed:** 2026-07-17

### Dynamic Progress Label Alignments

**What:** Split progress percent sign into separate label and call lv_obj_update_layout to prevent overlapping text.

**Why:** Prevents dynamic characters from colliding when shifting numbers.

**Context:** When the BLE telemetry updates the percentage value, the layout needs recalculation before snapping the "%" symbol to the right edge.

**Effort:** S
**Priority:** P0

**Completed:** 2026-07-17

### Chinese Font and Battery Telemetry Support

**What:** Enable CJK 16px font for Chinese rendering, poll battery status from AXP2101 PMIC, relocate resets time indicator badge left side.

**Why:** Satisfy user preferences for device battery, Chinese task names, and reset layouts.

**Context:** Enables CJK font tables in sdkconfig, updates ui.c layout, queries battery percent and charging state registers over I2C, and parses resets timestamps.

**Effort:** M
**Priority:** P0

**Completed:** 0.1.0.0 (2026-07-18)

