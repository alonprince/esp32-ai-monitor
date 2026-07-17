# Time Persistence & RTC Spec (ESP32-C6)

## 1. Scope / Trigger
- **Trigger**: Synchronizing time over BLE, keeping time on the device, and recovering time after software resets (watchdog resets, soft reboots, deep sleep wakeups).

## 2. Signatures
- **Persistent Time Storage**:
  ```c
  typedef struct {
      uint32_t magic;
      uint64_t sync_rtc_us; // RTC hardware counter at sync (microseconds)
      time_t sync_epoch;    // Synced epoch time (seconds)
  } persistent_time_t;
  ```
- **Sync API**:
  ```c
  void time_persist_sync(time_t epoch_sec);
  ```
- **Restore API**:
  ```c
  void time_persist_restore(void);
  ```

## 3. Contracts
- **RTC Memory Allocation**:
  - The persistent storage variable must be decorated with `RTC_NOINIT_ATTR` to ensure it is placed in the RTC slow memory section and not re-initialized to zero by the bootloader during software resets.
- **Hardware RTC Time Source**:
  - `esp_rtc_get_time_us()` must be used to get the monotonic microsecond counter from the RTC hardware controller. Do not use `esp_timer_get_time()` for interval calculations across reboots, as `esp_timer` resets to 0 on boot.
- **Time Sync Source**:
  - When a BLE time sync command (`0x07`) is received, the epoch time is set using `settimeofday()` and synced to RTC using `time_persist_sync()`.

## 4. Good/Base/Bad Cases
- **Good (RTC Persistence)**: System time is saved to `RTC_NOINIT` memory along with the hardware RTC counter. On reboot, `time_persist_restore()` calculates the elapsed time and restores the system clock. The screen correctly displays the time immediately after reboot.
- **Bad (System Time Reset)**: System time is synced but not stored in RTC. On soft reboot (e.g. crash or watchdog reset), the clock resets to 1970 and displays `--:--` until another BLE connection syncs it.

## 5. Verification
- Verify boot log outputs: `Restored time from RTC: <epoch> (elapsed <seconds> s)` after triggering a software reset (e.g. through a reset console command or watchdog trigger).
