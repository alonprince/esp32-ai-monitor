# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to Semantic Versioning.

## [0.1.0.0] - 2026-07-18

### Added
- Native Chinese typography support using the built-in Source Han Sans SC CJK 16px font for task names and approval command descriptions.
- Device battery monitoring polling the onboard AXP2101 PMU over I2C to display tiered battery icons and charging status symbol in the header.
- Standalone host-runnable C mock test suite under `test/test_firmware.c` to verify telemetry parsing and battery status UI choosing logic.
- Automated tests in `test/test_telemetry.py` to cover host JSON event hooks and telemetry handling logic.

### Changed
- Relocated the resets time indicator caption to the left of the orange badge in the footer dashboard layout.
- Swapped the header WiFi status icon with the new device battery percentage indicator.
- Updated `mac_host.py` to transmit raw Chinese characters over BLE instead of translating them to Pinyin.
