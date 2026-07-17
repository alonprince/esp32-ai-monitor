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
