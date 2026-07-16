# Display Hardware & Touch Calibration Spec (Waveshare ESP32-C6-Touch-AMOLED-2.16)

## 1. Scope / Trigger
- **Trigger**: Setting up display drivers, screen rotation, and capacitive touch panel coordinates alignment for Waveshare ESP32-C6-Touch-AMOLED-2.16 dev boards using QSPI AMOLED (CO5300/SH8601) and I2C touch (CST9217/CST9220) controllers.

## 2. Signatures
- **Display Driver Factory**:
  ```c
  esp_err_t esp_lcd_new_panel_co5300(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config, esp_lcd_panel_handle_t *ret_panel);
  esp_err_t esp_lcd_panel_init(esp_lcd_panel_handle_t panel);
  ```
- **Touch Driver Factory**:
  ```c
  esp_err_t esp_lcd_touch_new_i2c_cst9217(const esp_lcd_panel_io_handle_t io, const esp_lcd_touch_config_t *config, esp_lcd_touch_handle_t *ret_touch);
  ```

## 3. Contracts
### Display Initialization Command Sequence (`co5300_lcd_init_cmd_t`)
To rotate the screen physically in hardware:
- Register `0x36` (MADCTL) must be written with the value `0xA0` during display driver initialization:
  - `0xA0` = `0b10100000` (`MV = 1` - swap XY, `MY = 1` - mirror Y)
- Resolution Contract: `LCD_H_RES = 480`, `LCD_V_RES = 480`.

### Touch Driver Alignment Flags (`esp_lcd_touch_config_t.flags`)
Since the display is physically rotated 90 degrees and corrected in hardware using register `0x36 = 0xA0`, the touch driver flags must match this native coordinate layout:
- `swap_xy = 1`
- `mirror_x = 0`
- `mirror_y = 1`

## 4. Validation & Error Matrix
| Test Case / Condition | Expected Result / Behavior | Prevention / Fix |
|---|---|---|
| Screen initialized with default vendor commands (no `0x36 = 0xA0`) | Screen content appears rotated 90 degrees clockwise | Must supply a custom initialization command array specifying `0x36` with value `0xA0`. |
| Software-level rotation in LVGL (e.g. `lv_display_set_rotation(disp, ROTATION_270)`) | Causes rendering overhead during UI redrawing | Use hardware-level rotation instead; keep LVGL display rotation at 0 degrees (`LV_DISPLAY_ROTATION_0`). |
| Touch driver `.swap_xy = 0` | Swipe/click coordinates are inverted and mismatched relative to active drawing area | Restore `.swap_xy = 1` in `esp_lcd_touch_config_t`. |

## 5. Good/Base/Bad Cases
- **Good (Hardware level rotation)**: Display initialized with custom commands writing `0xA0` to command `0x36`. LVGL rotation is `0` (native). Touch driver configured with `.swap_xy = 1, .mirror_y = 1`. Rendering is fast and touch perfectly matches the visual display.
- **Base (Software level rotation)**: Display initialized with defaults (no custom commands, `0x36` default). LVGL software rotation set to `LV_DISPLAY_ROTATION_270`. Touch flags adjusted in software to match. Functional, but incurs LVGL software rotation drawing overhead.
- **Bad (Mismatched orientation)**: Display initialized with defaults (no hardware rotation). LVGL rotation is `0` (native). Touch `.swap_xy = 0`. UI layout is lying on its right side, and touch inputs register in completely wrong directions.

## 6. Tests Required
- **Boot and Output Verification**: Verify serial console outputs: `Installing CO5300 display driver...`, `Display initialized successfully!`, `CST9220 touch controller ready`.
- **Display Upright Check**: Verify UI layout is upright (top status bar aligns to the physical top of the board).
- **Touch Alignment Check**: Enable touch indicator or check click positions. Tap the four corners of the screen and ensure the touch coordinates map correctly to the active UI elements without drift or axis swapping.

## 7. Wrong vs Correct
#### Wrong (Software rotation fallback with wrong touch flags)
```c
// main/main.c
static void init_touch(void) {
    const esp_lcd_touch_config_t tp_cfg = {
        .flags = {
            .swap_xy = 0, // WRONG: disabled swap_xy makes coords completely wrong
            .mirror_x = 0,
            .mirror_y = 1,
        },
    };
    esp_lcd_touch_new_i2c_cst9217(..., &tp_cfg, ...);
}

static void lvgl_init(void) {
    lv_display_t *disp = lv_display_create(480, 480);
    // WRONG: Incurs software rendering overhead
    lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_270);
}
```

#### Correct (Hardware-level rotation matching Waveshare specification)
```c
// main/main.c
static const co5300_lcd_init_cmd_t custom_lcd_init_cmds[] = {
    {0xFE, (uint8_t []){0x00}, 0, 0},
    {0xC4, (uint8_t []){0x80}, 1, 0},
    {0x35, (uint8_t []){0x00}, 0, 10},
    {0x36, (uint8_t []){0xA0}, 1, 0}, // CORRECT: Set MADCTL to 0xA0 for hardware rotation (Swap XY, Mirror Y)
    {0x53, (uint8_t []){0x20}, 1, 10},
    {0x51, (uint8_t []){0xFF}, 1, 10},
    {0x63, (uint8_t []){0xFF}, 1, 10},
    {0x2A, (uint8_t []){0x00, 0x06, 0x01, 0xDD}, 4, 0},
    {0x2B, (uint8_t []){0x00, 0x00, 0x01, 0xD1}, 4, 0},
    {0x11, (uint8_t []){0x00}, 0, 60},
    {0x29, (uint8_t []){0x00}, 0, 0},
};

static void init_touch(void) {
    const esp_lcd_touch_config_t tp_cfg = {
        .flags = {
            .swap_xy = 1, // CORRECT: Match hardware-swapped screen coordinates
            .mirror_x = 0,
            .mirror_y = 1,
        },
    };
    esp_lcd_touch_new_i2c_cst9217(..., &tp_cfg, ...);
}

static void lvgl_init(void) {
    lv_display_t *disp = lv_display_create(480, 480);
    // CORRECT: Display is already rotated in hardware; no software rotation needed!
}
```
