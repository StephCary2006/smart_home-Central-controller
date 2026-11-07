# Phase 3: Dual Timekeeping & Scheduling Engine

> **Timeline:** Days 8–11 · **Prerequisites:** Phase 2 completed (all peripheral drivers working)

---

## 3.1 Objective

Implement the **dual-clock architecture** defined in the PRD:
1. **Monotonic Timer Engine** — countdown/interval timers driven by `esp_timer` (immune to NTP adjustments)
2. **SNTP Synchronization** — background NTP client that syncs POSIX wall-clock without blocking sensor tasks
3. **Wall-Clock Cron Scheduler** — absolute time event execution with NTP step compensation

```
                    ┌────────────────────┐
[NTP Sync] ────────▶│ POSIX Wall-Clock   │────▶ Cron / Scheduled Events
                    │ time() / gettimeofday()│
                    └────────────────────┘
                         (Step/Slew Adjust)

[Hardware XTAL] ───▶┌────────────────────┐
                    │ Monotonic Timer    │────▶ Countdown / Interval Timers
                    │ esp_timer_get_time()│
                    └────────────────────┘
                         (Strictly Invariant)
```

**Critical Design Rule:** Monotonic timers must NEVER use `time()`, `gettimeofday()`, or any wall-clock source. Wall-clock schedulers must NEVER use monotonic sources for absolute time calculation.

---

## 3.2 Task Breakdown

```
Phase 3
├── Task 3.1: Monotonic Timer Engine (esp_timer)        [Day 8–9]
│   ├── timer_engine.c/.h
│   ├── One-shot and periodic countdown timers
│   └── Buzzer callback on timer expiration
├── Task 3.2: SNTP Synchronization Client               [Day 9–10]
│   ├── sntp_sync.c/.h
│   ├── Background FreeRTOS task
│   └── Non-blocking error handling (Wi-Fi dropout safe)
└── Task 3.3: Wall-Clock Cron Scheduler                 [Day 10–11]
    ├── cron_scheduler.c/.h
    ├── Absolute wall-clock event execution
    └── NTP step compensation (re-calculate next trigger)
```

---

## 3.3 Detailed Task Guides

### Task 3.1: Monotonic Timer Engine — `timer_engine.c/.h`

**PRD Reference:** FR-1.1 (Monotonic Countdown Engine)

**Purpose:** Provide countdown and interval timers for relative time operations (e.g., "turn off relay in 15 minutes", "beep every 10 seconds"). These timers are driven by the hardware crystal oscillator via `esp_timer` and are **completely immune** to NTP clock adjustments.

**Interface Design:**

```c
// timer_engine.h
#ifndef TIMER_ENGINE_H
#define TIMER_ENGINE_H

#include "esp_err.h"
#include "esp_timer.h"
#include <stdint.h>
#include <stdbool.h>

// Maximum number of concurrent timers (statically allocated, NFR-3)
#define MAX_TIMERS 8

// Timer callback function type
typedef void (*timer_callback_t)(void *arg);

// Timer handle (index into static array)
typedef int timer_handle_t;

/**
 * Initialize the timer engine. Call once from app_main().
 */
esp_err_t timer_engine_init(void);

/**
 * Create a one-shot countdown timer.
 * @param name         Human-readable name (for logging)
 * @param duration_ms  Countdown duration in milliseconds
 * @param callback     Function to call when timer expires
 * @param arg          Argument passed to callback
 * @param handle       Output: handle to the created timer
 */
esp_err_t timer_engine_create_oneshot(const char *name,
                                      uint32_t duration_ms,
                                      timer_callback_t callback,
                                      void *arg,
                                      timer_handle_t *handle);

/**
 * Create a periodic (repeating) interval timer.
 * @param name         Human-readable name
 * @param interval_ms  Interval between triggers in milliseconds
 * @param callback     Function to call on each trigger
 * @param arg          Argument passed to callback
 * @param handle       Output: handle to the created timer
 */
esp_err_t timer_engine_create_periodic(const char *name,
                                        uint32_t interval_ms,
                                        timer_callback_t callback,
                                        void *arg,
                                        timer_handle_t *handle);

/**
 * Cancel and delete a timer.
 */
esp_err_t timer_engine_cancel(timer_handle_t handle);

/**
 * Get remaining time for a one-shot timer in milliseconds.
 * Returns 0 if timer has expired or is invalid.
 */
uint32_t timer_engine_remaining_ms(timer_handle_t handle);

/**
 * Get current monotonic time in microseconds.
 * Wrapper around esp_timer_get_time() for convenience.
 */
int64_t timer_engine_now_us(void);

#endif // TIMER_ENGINE_H
```

**Implementation Notes:**

1. **Use `esp_timer` API** (not FreeRTOS software timers):
   - `esp_timer_create()` — create a timer with callback
   - `esp_timer_start_once()` — one-shot countdown
   - `esp_timer_start_periodic()` — repeating interval
   - `esp_timer_stop()` / `esp_timer_delete()` — cancel

2. **Static allocation (NFR-3):**
   - Pre-allocate an array of `MAX_TIMERS` timer slots
   - Each slot contains: `esp_timer_handle_t`, name, status, callback info
   - `create` finds a free slot; `cancel` releases it

3. **Buzzer integration:**
   - On timer expiration callback, trigger `buzzer_ctrl` to beep
   - Example: relay auto-off timer → beep when relay turns off

4. **Critical invariant:** 
   - NEVER call `time()`, `gettimeofday()`, or any wall-clock function inside timer callbacks
   - Use ONLY `esp_timer_get_time()` for any time calculations

**Example Usage:**

```c
// In app_main or rule engine:
static void relay_timeout_cb(void *arg)
{
    gpio_set_level(RELAY_GPIO_PIN, 0);  // Turn off relay
    buzzer_beep(3);                      // Beep 3 times
    ESP_LOGI("TIMER", "Relay auto-off triggered");
}

// Start a 15-minute relay timeout
timer_handle_t h;
timer_engine_create_oneshot("relay_timeout", 15 * 60 * 1000,
                            relay_timeout_cb, NULL, &h);
```

**Acceptance Criteria:**
- [ ] One-shot timers fire exactly once after the specified duration
- [ ] Periodic timers repeat at the specified interval
- [ ] Timers are NOT affected by NTP clock step adjustments
- [ ] `timer_engine_remaining_ms()` returns accurate countdown values
- [ ] All timer slots are statically allocated (no `malloc`)
- [ ] Cancellation properly releases the timer slot

---

### Task 3.2: SNTP Synchronization Client — `sntp_sync.c/.h`

**PRD Reference:** FR-1.3 (Non-Blocking NTP Client)

**Purpose:** Run an SNTP daemon in a background FreeRTOS task that periodically syncs the system wall-clock from `pool.ntp.org`. Must be completely non-blocking — Wi-Fi dropouts or DNS failures must NEVER block sensor polling or monotonic timers.

**Interface Design:**

```c
// sntp_sync.h
#ifndef SNTP_SYNC_H
#define SNTP_SYNC_H

#include "esp_err.h"
#include <stdbool.h>
#include <time.h>

/**
 * Initialize Wi-Fi and start the SNTP background synchronization task.
 * @param wifi_ssid     Wi-Fi network SSID
 * @param wifi_password Wi-Fi network password
 */
esp_err_t sntp_sync_init(const char *wifi_ssid, const char *wifi_password);

/**
 * Check if the system clock has been synchronized at least once.
 */
bool sntp_sync_is_synced(void);

/**
 * Get the current wall-clock time as a formatted string.
 * @param buf      Output buffer
 * @param buf_len  Buffer size
 * @param format   strftime format string (e.g., "%Y-%m-%d %H:%M:%S")
 */
esp_err_t sntp_sync_get_time_str(char *buf, size_t buf_len, const char *format);

/**
 * Get the current wall-clock time as a POSIX time_t.
 */
time_t sntp_sync_get_time(void);

#endif // SNTP_SYNC_H
```

**Implementation Notes:**

1. **Wi-Fi Station Mode:**
   - Initialize `esp_netif`, `esp_event`, `esp_wifi`
   - Connect to the configured AP
   - Handle disconnection events gracefully (auto-reconnect)

2. **SNTP Configuration:**
   - Use `esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL)`
   - Set server: `esp_sntp_setservername(0, "pool.ntp.org")`
   - Set timezone: `setenv("TZ", "ICT-7", 1); tzset();` (GMT+7 for Vietnam)
   - Register a time sync notification callback

3. **Non-blocking architecture:**
   - SNTP runs in its own FreeRTOS task (priority `TASK_PRIORITY_SNTP`)
   - Use event groups or a simple `volatile bool` flag for sync status
   - If Wi-Fi is disconnected, the task simply waits and retries
   - Other tasks (sensors, timers) are completely independent

4. **Sync notification callback:**
   ```c
   static void time_sync_notification_cb(struct timeval *tv)
   {
       ESP_LOGI("SNTP", "Time synchronized: %s", asctime(localtime(&tv->tv_sec)));
       sntp_synced = true;
       // Notify cron scheduler to recalculate next trigger
   }
   ```

**Acceptance Criteria:**
- [ ] System clock syncs to NTP within 30 seconds of Wi-Fi connection
- [ ] `sntp_sync_is_synced()` returns `true` after first sync
- [ ] Sensor tasks and monotonic timers continue uninterrupted during NTP sync
- [ ] Wi-Fi dropout does NOT crash or hang any task
- [ ] Auto-reconnect works after Wi-Fi dropout
- [ ] Timezone correctly set to GMT+7

---

### Task 3.3: Wall-Clock Cron Scheduler — `cron_scheduler.c/.h`

**PRD Reference:** FR-1.2 (Wall-Clock Scheduler / Cron)

**Purpose:** Execute actions at specific absolute wall-clock times (e.g., "turn on relay at 06:00:00 GMT+7 every day"). Must handle NTP step adjustments by recalculating the next trigger delta.

**Interface Design:**

```c
// cron_scheduler.h
#ifndef CRON_SCHEDULER_H
#define CRON_SCHEDULER_H

#include "esp_err.h"
#include <stdint.h>

// Maximum number of cron jobs (statically allocated)
#define MAX_CRON_JOBS 8

// Cron callback function type
typedef void (*cron_callback_t)(void *arg);

// Cron job handle
typedef int cron_handle_t;

// Cron schedule structure (simplified)
typedef struct {
    int8_t hour;      // 0-23, or -1 for "every hour"
    int8_t minute;    // 0-59, or -1 for "every minute"
    int8_t second;    // 0-59, or -1 for "every second"
    uint8_t weekdays; // Bitmask: bit0=Sun, bit1=Mon, ..., bit6=Sat. 0x7F = every day
} cron_schedule_t;

/**
 * Initialize the cron scheduler. Requires SNTP to be initialized first.
 */
esp_err_t cron_scheduler_init(void);

/**
 * Register a new cron job.
 * @param name      Human-readable name
 * @param schedule  When to trigger
 * @param callback  Function to call when triggered
 * @param arg       Argument passed to callback
 * @param handle    Output: handle to the created job
 */
esp_err_t cron_scheduler_add(const char *name,
                              const cron_schedule_t *schedule,
                              cron_callback_t callback,
                              void *arg,
                              cron_handle_t *handle);

/**
 * Remove a cron job.
 */
esp_err_t cron_scheduler_remove(cron_handle_t handle);

/**
 * Notify the scheduler that a time step occurred (called from SNTP callback).
 * Forces recalculation of all next-trigger times.
 */
void cron_scheduler_on_time_step(void);

#endif // CRON_SCHEDULER_H
```

**Implementation Notes:**

1. **Core loop (in a FreeRTOS task):**
   ```
   while (1) {
       if (!sntp_sync_is_synced()) {
           vTaskDelay(pdMS_TO_TICKS(1000));  // Wait for sync
           continue;
       }

       time_t now = time(NULL);
       struct tm *tm_now = localtime(&now);

       for each cron_job:
           if matches(tm_now, job.schedule):
               job.callback(job.arg);

       // Sleep until next second boundary
       vTaskDelayUntil(..., pdMS_TO_TICKS(1000));
   }
   ```

2. **NTP step compensation:**
   - When `cron_scheduler_on_time_step()` is called (from SNTP callback):
     - Recalculate all "next trigger" times based on the new wall-clock
     - Avoid double-firing if the clock jumped forward past a trigger
     - Avoid skipping if the clock jumped backward

3. **Schedule matching logic:**
   ```c
   static bool cron_matches(const struct tm *now, const cron_schedule_t *sched)
   {
       if (sched->hour != -1 && sched->hour != now->tm_hour) return false;
       if (sched->minute != -1 && sched->minute != now->tm_min) return false;
       if (sched->second != -1 && sched->second != now->tm_sec) return false;
       if (sched->weekdays != 0x7F) {
           if (!(sched->weekdays & (1 << now->tm_wday))) return false;
       }
       return true;
   }
   ```

4. **Guard against double-fire:**
   - Track `last_fired_time` for each job
   - Only fire if `now > last_fired_time` (prevents re-fire on time step)

**Example Usage:**

```c
// Turn on relay every day at 06:00:00 GMT+7
static void morning_relay_on(void *arg)
{
    gpio_set_level(RELAY_GPIO_PIN, 1);
    ESP_LOGI("CRON", "Morning relay ON triggered at 06:00");
}

cron_schedule_t morning = {
    .hour = 6, .minute = 0, .second = 0,
    .weekdays = 0x7F  // Every day
};
cron_handle_t h;
cron_scheduler_add("morning_relay", &morning, morning_relay_on, NULL, &h);
```

**Acceptance Criteria:**
- [ ] Cron jobs fire at the correct wall-clock time
- [ ] Scheduler handles NTP step forward without double-firing
- [ ] Scheduler handles NTP step backward without skipping
- [ ] `cron_scheduler_on_time_step()` correctly recalculates triggers
- [ ] Scheduler waits gracefully until NTP first sync completes
- [ ] All job slots are statically allocated (NFR-3)

---

## 3.4 Integration: Combining Both Clock Domains

In `app_main()`, wire both clock systems together:

```c
void app_main(void)
{
    // Phase 1 & 2 init (I2C, sensors, etc.)
    i2c_manager_init();
    sht31_init();
    ssd1306_init();
    adc_light_init();
    pms7003_init();

    // Phase 3 init
    timer_engine_init();
    sntp_sync_init("YourSSID", "YourPassword");
    cron_scheduler_init();

    // Example: Monotonic timer — beep every 30 seconds
    timer_handle_t beep_timer;
    timer_engine_create_periodic("heartbeat_beep", 30000,
                                  heartbeat_beep_cb, NULL, &beep_timer);

    // Example: Cron job — turn on relay at 06:00 daily
    cron_schedule_t morning = { .hour = 6, .minute = 0, .second = 0, .weekdays = 0x7F };
    cron_handle_t cron_h;
    cron_scheduler_add("morning_relay", &morning, morning_relay_on, NULL, &cron_h);

    // Display loop
    while (1) {
        // Update OLED with current time + sensor data
        char time_str[32];
        sntp_sync_get_time_str(time_str, sizeof(time_str), "%H:%M:%S");
        ssd1306_write_line(0, time_str);
        // ... sensor data on other lines ...
        ssd1306_update();

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

---

## 3.5 Phase 3 Completion Checklist

| # | Checkpoint | Status |
|:---:|---|:---:|
| 1 | Monotonic one-shot timers fire accurately (±10ms) | ☐ |
| 2 | Monotonic periodic timers repeat at correct intervals | ☐ |
| 3 | Monotonic timers unaffected by NTP step adjustments | ☐ |
| 4 | Timer cancellation works correctly, frees slot | ☐ |
| 5 | Wi-Fi connects to AP successfully | ☐ |
| 6 | SNTP syncs time within 30s of Wi-Fi connection | ☐ |
| 7 | Timezone correctly set to GMT+7 | ☐ |
| 8 | SNTP failure/Wi-Fi dropout does NOT block other tasks | ☐ |
| 9 | Cron jobs fire at correct wall-clock times | ☐ |
| 10 | Cron handles NTP forward/backward step correctly | ☐ |
| 11 | OLED displays current NTP-synced time | ☐ |
| 12 | All timer/cron slots are statically allocated (NFR-3) | ☐ |

---

> **Next:** Proceed to [Phase 4: Integration, Automation & Stress Test](Phase_4_Integration_Stress_Test.md)
