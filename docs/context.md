# Project context

## README/source synchronization (2026-10-22)

- Root `README.md` was refreshed against current firmware for features, GPIO wiring, source architecture, peripherals/protocols, control techniques, build setup, and validation limits. It explicitly states that MQ-2 values are raw ADC counts despite the current `ppm` labels in web/Telegram output.
- Corrected the wiring guide's stale statement that D0 cannot affect relay, and updated the web protocol guide to distinguish command schema v1 from state snapshot schema v2, including fire fields.
- Documentation review only: no firmware changed and no build/tests run. Claims are based on current source/configuration; physical wiring, sensor calibration, and board behavior remain unverified.

## Web flame-status card (2026-10-22)

- User selected a fourth web status card. WebSocket state protocol is now version 2 and sends `fire_valid`, `fire`, and `fire_confirming`. The browser shows: `Phat hien lua` in red for a valid active fire; `Dang xac nhan het` in yellow while the 3-second valid no-fire period runs; `Khong phat hien` when valid D0 reports no fire; `Khong doc duoc D0` in gray for invalid D0; and `Chua dong bo` before a fresh WebSocket state. This is display-only and does not add a network connection or alter relay/Telegram policy.
- `app.js` rejects the former version-1 state message because it lacks the required fire fields. The page and JavaScript are served with `Cache-Control: no-store`, so a normal browser reload receives the matching version-2 assets. `node --check` and a deterministic Node schema test passed; ESP-IDF v5.5.4 build passed with the local Telegram configuration: `Gas_System.bin` 0x1049d0, app partition 0x177000, 31% free. Build log: `%TEMP%\\web_fire_status_idf_build.log`. No board/web-browser validation after this change.

## LCD fire-status suffix (2026-10-22)

- User selected an always-visible three-character D0 suffix at the end of LCD line two. `F:1` means the latest D0 read is valid and detects fire; `F:0` means the latest D0 read is valid and detects no fire; `F:?` means no valid D0 result is available, including startup before the first successful read and a D0 read failure. The status is display-only and does not change relay, Telegram, gas or fire-clear confirmation policy.
- Implementation stores `fire_read_valid` in the single-owner `control_state`, then `display_task` reserves columns 14..16 of line two for the suffix. The preceding line-two text is bounded to 13 characters before padding, so all current LCD states retain the suffix. Control-core host tests cover valid/error/valid D0 status transitions. ESP-IDF v5.5.4 build passed with the local Telegram configuration: `Gas_System.bin` 0x104400, app partition 0x177000, 31% free. Build log: `%TEMP%\\lcd_fire_status_idf_build_retry.log`. No board flash after this display-only change.

## Fire-clear confirmation and retained Telegram event (2026-10-22)

- Board log captured at `%TEMP%\\gas_fire_clear_busy_evidence_20261022.log` proved that the old implementation detected `HET_LUA` at 674960 ms but dropped its Telegram event at 674980 ms because the earlier fire request was still sending until 676790 ms. This was not a D0 detection failure.
- User selected: after an active fire alert, FIRE_CLEARED requires D0 to report no fire continuously for 3000 ms. A valid fire reading or a D0 read error cancels the confirmation and a later valid no-fire reading starts a new 3-second period. Relay behavior is unchanged: Auto reevaluates the relay immediately on a valid no-fire D0 reading; Manual retains user authority.
- User selected one retained event: while the Telegram worker is busy, one event containing `FIRE_CLEARED` is held and sent immediately after the in-progress request completes. A later `FIRE_DETECTED` cancels the held clear event because it is no longer true. Other busy events retain the prior drop behavior; worker/network I/O never blocks sensor, control, button or web tasks.
- Implementation: `control_core` stores confirmation state and timestamp; `telegram_notify_pending` is a pure helper protected by the existing worker state lock. GCC C11 host tests pass for the 3-second confirmation/error reset and held-clear/cancel-on-new-fire policy. ESP-IDF v5.5.4 build passes with local Telegram configuration: `Gas_System.bin` 0x1043a0, app partition 0x177000, 31% free. Build log: `%TEMP%\\gas_fire_clear_idf_build.log`. No board flash after this change.

## Gas hysteresis and flame detection implementation (2026-10-20)

- User authorized implementation of the previously agreed gas+flame relay and Telegram behavior. In Auto, a valid active flame D0 forces relay ON regardless of MQ-2 readiness, ADC validity, or gas threshold; without active flame, existing gas hysteresis decides. In Manual, relay remains under button/web authority. After flame clears, Auto immediately reevaluates gas policy; Manual keeps its manually selected output.
- D0 is sampled at the existing sensor task cadence (100 ms delay between samples) without extra debounce. A D0 read error is logged, treated as no active flame for relay evaluation so gas policy can continue, and does not clear the Telegram fire-notification latch. Only a valid no-flame read can produce FIRE_CLEARED.
- Control notifications now use a bit mask so simultaneous gas and fire transitions from one core event can be represented in a single Telegram message. Fire alerts and confirmed clear alerts are independent of Auto/Manual. Existing worker one-attempt/drop-when-offline-or-busy policy remains.
- `GAS_THRESHOLD_HYSTERESIS` is a `Gas_System -> Dieu khien` menuconfig integer, default 50, range 1..100 raw ADC counts. It is shared by Auto gas relay turn-off and Telegram gas recovery; recovery still requires three valid consecutive readings. `Gas_System/sdkconfig.defaults` selects the large single-app partition because the old 1 MiB partition was smaller than firmware with Telegram enabled. Note: this embedded control-core build requires hysteresis <= its minimum allowed gas threshold (100); the menu maximum 100 preserves that invariant.
- Added deterministic host tests under `components/control_core/test/test_control_core_fire_relay.c` and `components/telegram_notify/test/test_telegram_notify_format.c`. Both pass GCC C11 with `-Wall -Wextra -Werror -pedantic`. ESP-IDF 5.5.4 builds pass in temp copies with Telegram disabled (app 0x61a50) and enabled using dummy test credentials (app 0xa15b0); both fit the 0x177000 app partition. Build logs are `%TEMP%\\gas_flame_tel_hyst_disabled_build.log` and `%TEMP%\\gas_flame_tel_hyst_enabled_build.log`; host logs are `%TEMP%\\gas_flame_tel_hyst_core_test.log` and `%TEMP%\\gas_flame_tel_hyst_telegram_test.log`. The user's `Gas_System/build` was not used, and no hardware/Telegram delivery was tested.
- Existing local changes were present before this implementation. Preserve them; do not use broad git restore/reset/clean. Updated `README.md`, `docs/telegram-guide.md`, `docs/gpio-menuconfig.md`, `docs/codebase-map.md` and this context record.

## Telegram implementation status (2026-10-17)

- Part 3 is implemented: `Gas_System/components/telegram_notify` contains a separate FreeRTOS worker, one-event queue/busy gate, raw-ADC Vietnamese message formatter, HTTPS Bot API POST with CA bundle verification, bounded JSON response check, and sanitized logs. Native GCC message-format tests passed previously, then their test source was removed on 2026-10-19 at the user's request. The component is linked from `main/CMakeLists.txt`; enabling Telegram in Kconfig selects the certificate bundle.
- Part 4 is implemented: `control_core` tracks alarm state and returns a notification transition with ADC and effective threshold. Alarm detection is independent of Auto/Manual and relay state; recovery requires 3 consecutive valid samples <= threshold - hysteresis. Invalid data resets the recovery count without creating a sensor-fault notification. A draft threshold has no effect. After successful save, a newly lowered threshold can alarm immediately using the latest Ready sample; raising the threshold does not bypass the three-sample recovery rule. `control_task` writes structured transition logs.
- Part 5 is implemented: `app_main` starts the Telegram worker when enabled and passes its readiness to `control_task`; otherwise it logs that Telegram is disabled or unavailable and continues normal control startup. After the control core reports an alarm/recovery transition, `control_task` maps it to a Telegram event and calls the nonblocking submit API. The HTTP request remains in the notification worker, so the control owner does not wait for network I/O. Only the transition is submitted; sensor faults still do not generate Telegram messages.
- Part 5 validation: native GCC notification-policy and message-format tests passed with `-std=c11 -Wall -Wextra -Werror -pedantic`. ESP-IDF 5.5.4 builds passed in a temporary project copy with Telegram both enabled and disabled. Logs are `%TEMP%\gas_telegram_part5_enabled_build.log`, `%TEMP%\gas_telegram_part5_disabled_build.log`, `%TEMP%\gas_control_core_part5_test_run.log`, and `%TEMP%\gas_telegram_format_part5_test_run.log`. The source `Gas_System/build` was not used for these checks. No physical board, Wi-Fi connection or real Telegram delivery was tested; token and chat ID are blank in the temporary build.

## Telegram setup documentation (2026-10-17)

- Part 6 is documented in `docs/telegram-guide.md` and linked from `README.md`: BotFather setup, private-chat `/start`, safe numeric Chat ID lookup, menuconfig/build/flash/monitor steps, expected logs, delivery/drop behavior, and token limitations. Checked against current Kconfig and runtime source. Documentation-only change; no firmware build or board delivery test was run.

## Telegram test source cleanup (2026-10-19)

- Removed `Gas_System/components/telegram_notify/test/test_telegram_notify_format.c` and `Gas_System/components/control_core/test/test_control_core_notifications.c` at the user's request. Neither file was listed in a component `idf_component_register(SRCS ...)` source list, and no remaining source references were found. Firmware build inputs are unchanged; the regression tests can no longer be rerun unless recreated.

## Telegram notification design discussion (2026-10-15)

- User prefers Telegram Bot instead of Twilio SMS. Confirmed requirements: put bot token and chat ID in menuconfig, send both an over-threshold alert and a recovery message, apply alerts in Auto and Manual, and confirm recovery after three consecutive valid samples reach the recovery boundary. Updated delivery policy: if Internet is unavailable, or a send fails despite Internet connectivity, drop that event, write an error log for send failures, and do not retry or deliver it later. User approved staged implementation; parts 1-3 are complete, with alarm policy and runtime wiring still to be implemented.
- Current firmware uses Wi-Fi STA and has an existing network component; Telegram requires Internet access and an outbound HTTPS Bot API request. Recipient must start/contact the bot before it can send a private message. Bot token must not be committed or printed in logs.
- Proposed architecture: control owner detects alarm/recovery transitions and passes compact events to a separate notification worker; HTTPS I/O must not block control or relay handling. The worker attempts delivery only when Internet is available; failed/offline events are discarded, with no RAM queue, retry, or deferred delivery. If gas remains high when Internet returns, no delayed alert is sent; a new observed transition is needed. Message must label sensor value as raw ADC, not ppm.
- Recovery boundary is at or below threshold minus the existing hysteresis (50 ADC counts). Three consecutive valid samples are required for recovery.
- ESP-IDF menuconfig saves values to `sdkconfig`. Part 1 now keeps local `Gas_System/sdkconfig` and `sdkconfig.old` files on the user's machine but removes them from Git tracking and ignores them; `sdkconfig.defaults` contains reproducible defaults with Wi-Fi credential strings blank. The prior tracked sdkconfig revisions remain in Git history; if their nonempty Wi-Fi credential fields were real, rotate them or clean repository history separately. A Telegram token compiled into firmware remains extractable from the device image.

## Telegram implementation plan (updated, 2026-10-17)

- Current source confirms a single control owner (`components/app_tasks/control_task`) and network startup before the web server. The control core already turns Auto relay on at `sample >= threshold` and off at `sample <= threshold - threshold_hysteresis` (currently 50 ADC counts); this output behavior cannot be used as the Telegram alarm state because notifications must work in both Auto and Manual.
- Staged implementation sequence: (1) isolate local credentials from Git; (2) add Telegram Kconfig entries; (3) add a standalone HTTPS notification task; (4) implement pure alarm/recovery transitions; (5) wire the worker into control/startup; (6) document BotFather setup, menuconfig, monitor logs and credential limits. Parts 1-5 are implemented and build/host-test validated; part 6 documentation is now complete. User wants each part explained before continuing.
- Confirmed ISSUE 1:A: recovery boundary is the existing configured threshold minus 50 ADC counts, and recovery requires three consecutive valid samples at or below that boundary.
- Confirmed ISSUE 2:A: do not send Telegram messages for sensor faults; keep those faults on the existing LCD/log path. Telegram scope is gas-over-threshold and gas-recovery events.
- Confirmed ISSUE 3:A: keep Telegram token/chat ID in menuconfig but ensure the user's secret-bearing `sdkconfig` is not tracked by Git. Explain that a token compiled into firmware remains extractable. Implementation must preserve a blank/default configuration path for clean checkouts and document local configuration.
- Network behavior: Wi-Fi `GOT_IP` only indicates local network address; it does not prove Telegram Internet reachability. Treat an HTTPS request failure as a failed event, log it without credentials, and discard it. The worker design must ensure busy/queued events are not later replayed as deferred events.
- Plan acceptance criteria: alarm event is generated on a valid sample at/above the active threshold, regardless of Auto/Manual or relay state; recovery requires three consecutive valid samples at/below active threshold minus 50 ADC, with an intervening higher/invalid sample resetting the recovery count. Sensor faults do not generate Telegram messages. The Telegram worker makes one HTTPS attempt per accepted event; if disconnected, busy, transport/HTTP/API failure, or queue admission fails, it logs a sanitized reason and drops the event. No automatic or deferred retry.
- Plan defaults: Telegram feature is disabled until explicitly enabled and both credentials are configured; only committed/effective threshold changes affect alarm classification (draft edits do not); a threshold change that changes the current alarm classification generates the corresponding transition event. Message text is Vietnamese and identifies the measurement as raw ADC. Bot must be started by the recipient before private delivery.
- Plan phases: (1) preserve non-secret build defaults while ignoring local `sdkconfig`; (2) add Kconfig; (3) implement the standalone HTTPS worker; (4) implement and test the alarm state machine; (5) wire state transitions and worker startup; (6) document BotFather setup, `/start`, chat ID, monitor logs and credential limits. Parts 1-6 are complete; no Telegram implementation-plan phase remains.
- Part 1 completed: added `Gas_System/sdkconfig.defaults` from the current non-secret configuration assignments, blanked credential-like string defaults (including Wi-Fi SSID/password), added `/sdkconfig` and `/sdkconfig.old` to `Gas_System/.gitignore`, and removed both local config files from Git tracking without changing their local contents. Verified local hashes unchanged, no nonempty sensitive strings in defaults, ignore rules apply, and a clean temporary copy generated `sdkconfig` from defaults and built successfully with ESP-IDF 5.5.4. Original baseline build also succeeded. The shell's clean reconfigure emitted a nonfatal ESP-ROM GDB-init warning because the export environment was not fully loaded; generation and build completed. No Telegram firmware code added yet.
- Part 2 completed: `main/Kconfig.projbuild` added `GAS_TELEGRAM_ENABLED` (default off), `GAS_TELEGRAM_BOT_TOKEN` and `GAS_TELEGRAM_CHAT_ID` (both empty strings, shown when notifications are enabled). At the completion of Part 2, no HTTP/task runtime logic existed. A clean temporary project parsed disabled and enabled configurations with blank credentials and built under ESP-IDF 5.5.4.
- Beginner walkthrough requested: explain each completed part, its file/task flow, validation and next part before continuing. No approval is implied for later parts beyond the user's staged-coding instruction; pause after each part for explanation.

## SMS notification discussion (2026-10-15)

- User selected an Internet SMS gateway architecture (1B), one alert on threshold crossing with re-arm after recovery (2A), and alerts independent of Auto/Manual mode (3A). No implementation authorized yet; next design detail is recipient country/number region and an anti-repeat limit/cooldown.
- Discussed separating the control owner from blocking notification I/O: the control task should emit an alert transition to a notification worker/backend path rather than call an SMS API synchronously. SMS payload must identify the value as raw ADC, not calibrated ppm.
- Current Twilio research: trial plans have recipient/geographic restrictions and published trial quotas; throughput and billing depend on message segments, sender, route and destination. Twilio's Vietnam guidelines require registered Alphanumeric Sender IDs/templates and state delivery receipts are not guaranteed. Verify the specific recipient country and current account/country requirements before implementation. No code changed.

## README overview update (2026-10-14)

- Reworked root README with current project features, listed hardware/software, MCU peripherals and ESP-IDF services, default GPIOs, setup commands, and ADC/ppm and hardware-validation limitations.
- Verified README claims against current `Gas_System` source, `sdkconfig`, Kconfig and wiring documentation. Removed a stale host-test command because `Gas_System/tests` and `Gas_System/tools` are absent from the current source tree; linked the test matrix as documentation and stated that automated tests are not currently present.
- Did not change firmware or run build/tests; verified local README links and Markdown diff only.

## Beginner syntax walkthrough for current control startup (2026-10-13)

- Explained current implementation at control_task.c:307 with current header alias from control_task.h:8: forward declaration `typedef struct control_service control_service_t`, full private struct definition in C file, `control_service_t **out`, and `const control_task_config *config`.
- Covered include/header guards, function prototype vs definition, pointer/member operators (`*`, `&`, `->`), callback function pointer, boolean/null/error expressions, `sizeof(*service)`/calloc, assignment/copy of config, RTOS object creation and rollback, relay OFF initialization, snapshot, task launch and publishing output handle only on success. Call site: main/Gas_System.c:96-97.
- Clarified invalid argument returns before `*out = NULL`; later failures leave out NULL and cleanup allocated RTOS resources. Config is copied shallowly, so callback context lifetime matters. No code or build changes; docs/context updated only.

## Current control startup source names and argument order correction (2026-10-13)

- Re-read current `control_task_start` after naming cleanup: current public alias is `control_service_t`, signature is `control_task_start(control_service_t **out, const control_task_config *config)`, called with `control_service_t *service = NULL; control_task_start(&service, &setup)`.
- Precise detail: argument validation happens before `*out = NULL`; therefore on invalid `config` or missing `set_relay`, the caller's output variable is left as-is. For valid arguments, the function clears output, allocates/initializes service, creates RTOS objects, calls relay OFF, publishes initial snapshots, creates owner task, then writes `*out` only on success. This corrects the prior shorthand that implied `*out` is always nulled before all validation.

## Opaque handle alias naming (2026-10-13)

- Changed the forward-declared opaque service/task handle aliases to end in `_t`: `control_service_t`, `sensor_task_t`, `display_task_t`, and `storage_task_t`. The `struct` tags and runtime behavior are unchanged; declarations, function prototypes, and all firmware call sites use the new aliases consistently.
- Purpose: visually distinguish the C `struct` tag (for example, `struct control_service`) from the typedef alias (`control_service_t`) for beginner readability. No wire protocol or persisted data format changes.

## `control_task_start` beginner explanation refresh (2026-10-13)

- Re-read the current English-named implementation after source rename. `control_task_start(control_service_t **out, const control_task_config *config)` uses `out` as output-handle location while returning `esp_err_t` separately. Caller declares `control_service_t *service = NULL`, passes `&service`; implementation sets `*out = NULL` first and publishes allocated opaque service only after core/queues/semaphore, relay OFF, snapshot and FreeRTOS task initialization succeed.
- Failure paths: invalid arguments -> `ESP_ERR_INVALID_ARG`; calloc or RTOS allocations/task failure -> `ESP_ERR_NO_MEM`; core validation -> `ESP_ERR_INVALID_ARG`; relay callback error propagates. Partial RTOS objects and service allocation are released by `cleanup_init_error`. Successful service is long-lived; no delete API. Config is shallow copied, so callback context must outlive it/service.
- Explanation covers pointer levels and memory diagram, why status+output handle are combined, API opacity, embedded heap fragmentation/exhaustion and lifetime tradeoffs. No code changes/build/tests.

## `control_task_start` and double-pointer explanation (2026-10-11)

- Read current `control_task.c/.h`, call site in `main/Gas_System.c`, and current codebase map. Explained `control_service_t **out` as the address of the caller's handle variable, allowing the function to assign the newly allocated opaque service while returning a separate `esp_err_t` status.
- Traced initialization order: validate args -> set caller output NULL -> calloc service -> copy config -> initialize core -> create queues/semaphore -> command relay OFF -> publish initial snapshots -> create FreeRTOS task -> assign `*out` only on success. Each failure path returns an ESP error and frees any created RTOS objects/service.
- Explained benefits/tradeoffs in embedded C: output handle + status return, opaque API, failure leaves predictable NULL; costs indirection and caller must pass address of a pointer, plus calloc heap fragmentation/exhaustion/lifetime. Callback context is shallow-copied and must outlive service. No source changes or build/tests.

## Web host and Vietnamese source comments audit (2026-10-07)

- `components/network/network.c` already logs `url=http://<ESP32-IP>` on `IP_EVENT_STA_GOT_IP`; `components/web_service/web_task/web_task.c` logs the HTTP server port and `/ws` when it starts. There is no fixed host because the router assigns the IP. Current checked-in `sdkconfig` has empty Wi-Fi SSID/password, and `components/network/include/wifi_local.h` is absent, so configure Wi-Fi in menuconfig or create the ignored local header before expecting an IP/web access.
- Scanned all 33 firmware C/header files as UTF-8. Restored 27 Vietnamese comments that were actually mojibake (including the earlier LCD comments); changed one mixed-language MQ-2 comment to Vietnamese. Confirmed no suspicious mojibake markers or invalid UTF-8 remain in firmware comments. No runtime code changed; no build/flash was needed or performed.

## Intern source-reading guide requested (2026-10-07)

- User asked for a concise, beginner-friendly explanation of how to read the complete embedded source, where to start, and detailed explanations only for difficult syntax/logic. This turn reviewed the current application-owned source under `Gas_System`: `main/Gas_System.c`, component C/headers, web HTML/CSS/JS, project/component CMake, Kconfig and saved sdkconfig. Generated `build/`, `.cache/` and test output were excluded. No firmware behavior was changed.
- Current recommended order: `main/Gas_System.c` startup/composition root -> `components/control_core/include/control_core.h` data model and `control_core.c` pure policy -> `components/app_tasks/control_task/control_task.c/.h` state ownership/queues/side effects -> sensor, button, storage and display tasks -> hardware drivers and NVS -> network -> web gateway/task/browser assets -> board pin/Kconfig/CMake configuration.
- Current runtime is hardware-only. Old sections in historical guides/context may refer to `dk_*`, `control_task_noi_bo.h`, fake/demo runtime, old test runners or earlier missing modules; current firmware identifiers are English (`control_*`, `sensor_*`, etc.), fake runtime was removed, and control/storage private headers were consolidated. Check the source before relying on older prose.
- Core concepts to teach: FreeRTOS tasks and bounded queues; single-owner mutable state; event/result structs and callback/context lifetime; uint64 boot/sequence/revision IDs; sample validity, moving mean and warmup; Auto hysteresis (`threshold` ON, `threshold - 50` OFF); asynchronous NVS save transaction and retained result; snapshot mailboxes; WebSocket input validation and stale-command checks; LCD nibble/PCF8574 output.
- Evidence/limits: source inventory counted 48 non-generated project files; application walkthrough was read-only. No build, tests, flash or physical validation was requested or performed. `docs/codebase-map.md` already contains the current runtime/module map but also historical records; treat its current-runtime sections as orientation and verify details against source.

## English embedded identifiers and Vietnamese comments (2026-10-01)

- User requested English names for firmware code while retaining Vietnamese explanations. Renamed C types, functions, constants, fields and local variables under `Gas_System/main` and `Gas_System/components`; browser HTML/CSS/JavaScript and the wire JSON field names remain unchanged. Added concise Vietnamese comments at the `app_main`, control-core configuration/state/event API, GPIO policy and control-task configuration boundaries.
- Verification: ESP-IDF v5.5.4 `idf.py build` passed; output `Gas_System.bin` is 0x612e0 bytes with 62% app partition free. No device was flashed or electrically tested.

## Full source walkthrough requested (2026-10-01)

- User is learning as a beginner and requested a current-source, hardware-only walkthrough. Re-read all application source under `Gas_System/main` and `Gas_System/components`, including web assets and build/config files. The explanation should follow the real runtime: `app_main` -> driver initialization/NVS -> control owner -> producer/consumer tasks -> Wi-Fi/HTTP/WebSocket -> browser; historical demo notes below are not current behavior.

## Removed fake runtime (2026-10-01)

- User authorized deleting fake/demo runtime data. Removed the `DK_THU_NGHIEM` menu option and saved configuration, plus fake LCD, ADC and relay callbacks from `Gas_System/main/Gas_System.c`. Firmware now always initializes and uses the real MQ-2, relay, buttons and LCD drivers.
- `idf.py build` with ESP-IDF v5.5.4 passed; `Gas_System.bin` was generated. No firmware was flashed and no physical-board test was performed.
- Saved `MQ2_ON_DINH_GIAY` remains `0`; before an Auto-mode hardware test, set the measured MQ-2 warm-up time in `menuconfig`, otherwise the sensor intentionally never reaches Ready state.

## Main-source teaching walkthrough (2026-09-29)

- Explained `main/Gas_System.c` from includes through `app_main()`: compile-time demo/hardware callbacks, control-policy setup, boot/session ID, hardware initialization, NVS restore, service/task creation order, callback/context lifetime, LCD configuration and asynchronous network/web start. Focused on why each function receives pointers, config structs and callbacks. No source changes or validation run.
- Follow-up requested a beginner-level account of task creation. Read exact `xTaskCreate` sites and will explain: main calls each `*_bat_dau`, the task is actually created inside that function, it may run immediately after successful creation, and all task contexts must outlive `app_main`. No source change.

## Beginner-readable source layout (2026-09-29)

- User authorized formatting all application source: 33 C/header files and app.js, plus HTML/CSS and simple component CMake registrations. Expanded compact statements/functions/enums, designated initializers and startup sections; added Gas_System/.clang-format for baseline 4-space, 100-column formatting. No policy/API/pin/configuration changes intended.
- Verification: C/header non-whitespace content and literal comparison (normalizing adjacent C strings and macro line continuations), JS whitespace comparison + node --check, HTML tag/attribute/nonblank-text comparison, CSS/CMake token comparison passed. Backups/check evidence in TEMP/gas_format_d2zczr1z. Final ESP-IDF 5.5.4 build passed with current sdkconfig (TEMP/gas_lcd_mapping_build.log). No physical-board test.

## Folder-by-folder teaching (2026-09-29)

- User requests progressive explanation of the whole source, folder by folder. Started with repository/module overview and main/: Gas_System.c startup, demo/hardware callbacks, policy parameters, boot identity, NVS restore, task lifetime/startup order, network/web, Kconfig, CMake and interview.md. Next teaching module: components/control_core, then app_tasks, drivers, storage, network and web_service. No firmware changes or validation run in this explanation.

## Monitor logging audit (2026-09-27)

- Audited log sites in app tasks, drivers, storage, network, web service and main. Added named debounced button events, human-readable mode/sensor/edit/relay state, startup state, successful relay callback, processed web command outcome, threshold classification changes including Manual/unknown, HTTP startup port and Wi-Fi configuration/disconnect diagnostics. Existing IP URL, storage and LCD error/recovery logs retained. No credentials/payloads logged.
- Sensor INFO logs now first sample/every 1000 ms/validity-or-delivery transition; normal sample control logs suppressed. Sampling and control decisions unchanged. Current sdkconfig default/max log level is INFO. Added monitor guide to main/interview.md.
- Host GCC checks passed for log cadence/fault/recovery/queue-error and threshold boundaries/Manual/unknown. Final ESP-IDF 5.5.4 build passed with current sdkconfig; log TEMP/gas_lcd_mapping_build.log. No board test.

## LCD driver teaching walkthrough (2026-09-27)

- Read current LCD protocol and I2C driver with line numbers. Explained serial PCF8574 port writes versus parallel LCD nibble/control signals, mapping, callbacks, initialization, frame writes, delays/error propagation and main/display integration. Example character A uses port bytes 49/4D/49 then 19/1D/19 (hex) with default mapping and active-high backlight. No code changes/build/board tests.

## LCD mapping menu simplification (2026-09-27)

- Removed LCD_SO_DO from main/Kconfig.projbuild at user request. Main now calls existing driver lcd_so_do_mac_dinh(), replacing hex decoding. Mapping stays P0=RS, P1=RW, P2=E, P3=BL, P4..P7=D4..D7; backlight polarity option preserved with explicit true/false branches. SDA/SCL and address options unchanged. Custom old LCD_SO_DO settings no longer apply.
- Updated driver comments and main/interview.md. Host GCC assertions verified default masks match the former 0x76543210 mapping; IDF 5.5.4 build passed with current sdkconfig (log in TEMP/gas_lcd_mapping_build.log). No board validation.

## Current-source embedded walkthrough (2026-09-27)

- Read all application C/header sources in Gas_System/main and components, web HTML/JS/CSS, component CMake files and project Kconfig; checked selected saved sdkconfig values. Explained startup, task ownership, queues, sensing/filtering, Auto/Manual hysteresis, shared draft/save transaction, LCD, Wi-Fi/WebSocket and native deployment. No firmware/config edits, new build, test run or flash in this turn.
- Saved sdkconfig currently selects ESP32, demo enabled, real MQ-2 warmup 0, web enabled, LCD address 0x27/manual selection, default GPIOs and FreeRTOS 100 Hz. Demo still uses real NVS. Hardware warmup 0 prevents Ready; 600 is an initial draft, not an automatically valid threshold.
- Sensor delays 100 ms after each read; timing is not a fixed 100 ms sampling period. Control yields one tick under continuous work (10 ms with current sdkconfig). Save timeout marks unknown and retains the transaction. Manual retains relay state on sensor fault; entering Auto reevaluates from OFF.
- tools/tests currently have no runner files; older README/helper/test instructions are historical. Current explanation uses native idf.py. Existing UI missing-threshold wording and unchanged-frame LCD detection limitations remain present; no fixes requested or performed.
- Refreshed the current operating snapshot at the top of docs/codebase-map.md; older sections below it are historical.

## Menuconfig explanation (2026-09-27)

- Explained current Gas_System menu from main/Kconfig.projbuild and consuming source: demo vs hardware, MQ-2 readiness delay, LCD address/mapping/backlight, eight GPIO assignments, Web LAN and Wi-Fi credentials with local-header precedence. Defaults are Kconfig defaults, not a claim about saved sdkconfig. No firmware/config changes or build performed.

## User build log diagnosis (2026-09-27)

- Attached log ends in CMake generation failure: ninja -t restat build.ninja reports failed recompaction: Permission denied. Earlier fullclean reported WinError 32 on build/.ninja_log. Evidence indicates build-metadata access failure; exact locking process or permission cause not established. No firmware edits, process termination, deletion or new build performed. Suggested closing concurrent build tasks and using a separate build directory if needed. git describe bad revision HEAD is not the stopping error (app version falls back to 1).

## Storage task consolidation (2026-09-27)

- User authorized merging storage_task_noi_bo.h. Removed the header; storage_task.c includes storage_task.h directly and luu_chay_mot_luot is static. Public header and function body unchanged, including retaining pending results on queue timeout without repeating NVS writes. No external callers found in firmware sources.
- ESP-IDF 5.5.4 build passed; log: Gas_System/build/refactor_checks/firmware.log. No remaining references to the removed header in components/main. Folder contains only storage_task.c/.h. Updated interview.md; no physical-board test.

## Control task consolidation follow-up (2026-09-27)

- User authorized merging control_task_noi_bo.h. Removed that header; control_task.c now includes the public header and FreeRTOS.h directly. dk_chay_mot_luot and dk_so_tick_cho are static, with unchanged bodies. Public control_task.h unchanged; storage_task not modified.
- Confirmed no remaining source includes of the removed header; folder contains only control_task.c/.h. Updated main/interview.md. ESP-IDF 5.5.4 build passed (build/refactor_checks/firmware.log); no on-board test. This supersedes the earlier decision to retain the control internal header.

## Current authorized consolidation (2026-09-27)

- User authorized and implemented: debounce into button_task.c; ADC filter and internal step declaration into sensor_task.c; LCD formatter and internal step declaration into display_task.c. Helpers/steps are now static. Public task headers retained; app_tasks CMake updated. LCD protocol/I2C split and control_task_noi_bo.h retained.
- All project Kconfig declarations moved to main/Kconfig.projbuild under Gas_System. Existing symbols/defaults preserved; added GAS_WIFI_SSID/GAS_WIFI_PASSWORD with wifi_local.h precedence.
- Fourteen component README files consolidated into main/interview.md and removed from original locations. Historical content is explicitly labeled; old tools/tests runners are absent.
- Validation: ESP-IDF 5.5.4 build succeeded with current sdkconfig; deterministic GCC debounce/ADC assertions passed; all three moved helper implementations verified unchanged except static. Build and host logs are in Gas_System/build/refactor_checks. No physical-board validation. Existing web/LCD review findings remain open.
- This entry supersedes older statements that consolidation was not authorized. Generated build files and sdkconfig were refreshed by IDF build.


Updated: 2026-09-27. Active role: Reviewer.

## Latest review (2026-09-27)

- Latest user asks file-by-file explanation and consolidation options, not implementation. Current inventory shows tools/ and tests/ directories empty; previous helper/test sources are no longer present. No deletion by this explanation turn. Existing guides referencing helper commands now describe removed tooling. Firmware source remains in main/components. Proposed only: centralize module READMEs, optionally internalize four *_noi_bo.h APIs and merge task-local debounce/filter/formatter pairs; retain core vs RTOS owner, drivers vs tasks, NVS vs worker, Wi-Fi vs HTTP/JSON boundaries. No merges or new verification performed.

- Confirmed app_tasks/web_task contains only README.md pointing to web_service/web_task/web_task.c. It is not registered in app_tasks CMake and can be removed without affecting firmware; actual web task under web_service must remain. User asked its purpose; no deletion performed.

- Follow-up explanation requested specifically for tools: traced board.local.json -> prepare_firmware.py -> config/sdkconfig.board + build_ready/generated/wifi_local.h -> firmware.ps1 IDF arguments -> network CMake/real-hardware branches in main. Deleting tools does not affect a flashed device; breaks documented helper commands and requires equivalent manual IDF/configuration. Native source sdkconfig still defaults demo, and absent Wi-Fi header returns ESP_ERR_NOT_FOUND. No source changes or deletion.

- User requested whole-project issue review and explanation whether tests/tools can be removed, not deletion. Reviewed source paths and CMake, reran all host suites successfully; no firmware code changes or new IDF build in this review.
- Confirmed P2 UI issue with Node VM probe: sensor Ready + config=0 shows missing threshold but still says below threshold (app.js:31–34). Core Auto OFF is unaffected. Log tests/web/out/review_missing_threshold.log.
- Confirmed LCD cache blind spot with fake output fault: unchanged frame skips output and returns success (display_task.c:29). Separate LCD power-cycle recovery is not guaranteed if software retains san_sang=true; physical behavior unverified. Log tests/display/out/review_display.log. Not fixed in review.
- tests/tools are PC-side, absent from firmware CMake source graph. tests removal does not block handoff tools/native build; tools removal requires replacing configuration generation/build commands manually. Existing source sdkconfig defaults demo and personal JSON is not automatically consumed by native IDF. Recommend keep both for development/CV. No folders removed.
- Detailed findings, limits and folder dependency explanation: docs/review-2026-09-27.md. Earlier “code complete” means agreed modules implemented, not zero defects; these findings remain open.

## Latest: configurable GPIO and cleanup (2026-09-27)

- User authorized adding menuconfig pin selection and removing files unrelated to project. Asked optional cleanup scope A generated outputs vs B tests/docs too; no selection received, proceeded with stated conservative A (generated build/cache/out only), preserving all project source, tests, tools, docs, Git and personal config. No inferred authorization to discard source/tests/docs.
- drivers/Kconfig.projbuild adds eight GAS_GPIO_* integer settings, existing pin defaults retained. board_pins.h rejects invalid IO/ADC1 pins and all pairwise collisions at compile time; maps GPIO32/33/34/35/36/39 to ADC1 CH4/5/6/7/0/3. GPIO policy excludes flash/UART0/strapping/input-only for outputs/pull-up buttons. Drivers consume config; main logs actual pin assignments.
- firmware.ps1 adds Menuconfig action. Personal SDKCONFIG now config/sdkconfig.board outside build (ignored), prepare_firmware preserves existing GPIO across Build/Flash. JSON still owns warmup/LCD settings/Wi-Fi. Test profile retains build_ready_test/sdkconfig.ready. docs/gpio-menuconfig.md explains both native idf.py and handoff paths.
- Verification: all host groups pass, including 32 negative/positive compiler cases and actual driver suites under alternate pins; profile persistence tests pass. Two full ESP-IDF 5.5.4 builds (default and all eight alternate GPIOs) pass at 0xd3bd0 bytes, 17% app partition free. Details retained in docs/verification-2026-09-27.md; generated logs/binaries removed in cleanup. No flash/physical validation.

- 2026-09-27 read-only integration verification: Gas_System.c includes all required driver/task headers and wires relay, buttons, MQ-2 ADC, LCD I2C callbacks, NVS, network and web into app_main. Peripheral loops run in their tasks; LCD I2C initialization is deferred to display callback. Hardware branches require DK_THU_NGHIEM off (handoff profile does this). No code changes or new test run for this clarification.

## Firmware handoff — latest authorized scope (2026-09-26)

- User requests finishing all firmware so they can wire and flash, not another sequence of modules or a prerequisite PC simulator. All previously agreed functional modules are implemented/integrated; remaining on-board acceptance cannot be claimed without hardware. No further permission needed for code. User did not understand the warmup question; explained that no guessed number is required now, retain existing 0/unconfigured behavior and expose it in personal configuration. Do not treat their clarification as approval of a numeric warmup.
- Added config/board.example.json, tools/prepare_firmware.py and tools/firmware.ps1. Personal ignored config/board.local.json supplies Wi-Fi, warmup and LCD settings. Generator validates strict keys/types/byte lengths, escapes C strings, restricts outputs to build_ready/build_ready_test, emits private generated Wi-Fi header and separate sdkconfig with real hardware + web. Source sdkconfig untouched. Network CMake explicitly prioritizes generated header. Build/Flash/Monitor actions; explicit COM port required, no flash/erase performed by agent.
- Optional LCD_TU_TIM probes only the 16 PCF8574/A addresses; selects exactly one ACK, returns explicit not-found/ambiguous/bus error otherwise. Display retries retain bus. Pin mapping is still a configurable physical assumption, not auto-detected. Default handoff enables scan, existing demo/manual-address profiles remain supported. Main initializer now names control configuration fields.
- Verified complete linked hardware/Wi-Fi/web build with non-secret fixture: tests/build_firmware.ps1 -ThuBanGiao, build_ready_test/Gas_System.bin 0xd3b20 bytes, 17% of 1 MiB app partition free. Link map includes esp_wifi_init, network, web and LCD. CONFIG_DK_THU_NGHIEM off, GAS_WEB and LCD_TU_TIM on, warmup 0. No actual connection/flash. This supersedes earlier reduced-size builds without Wi-Fi credentials.
- tests/run_all.ps1 passes all host groups: core 123, runtime 327, debounce 20, GPIO 28, storage 140, sensor 234, display 2957, gateway 142, network 119 = 4090 C assertions; Node UI scenarios and four Python profile tests pass. Logs retained in each tests/*/out; firmware_ready.log records full build. Personal credentials/build paths verified gitignored.
- README.md and docs/flash-guide.md are the primary handoff entry. Firmware code complete for agreed feature scope; next action is user enters local Wi-Fi, builds, wires and flashes, then measures/sets MQ-2 warmup and threshold and confirms backpack mapping. Auto remains inhibited with unconfigured warmup or no valid stored threshold. No claim of calibrated ppm, physical reliability or measured realtime latency.

## Latest implementation — Wi-Fi/web (supersedes older next-step notes)

- Implemented components/network (STA, retry timer, explicit setup errors, ignored local credentials header) and components/web_service (HTTP assets, WebSocket /ws, strict gateway, Vietnamese four-button UI/slider). No board required for host tests. No credentials configured; firmware skips network startup until local header is provided. Build sizes without credentials do not establish linked size with Wi-Fi configured.
- control_task remains sole state owner; adds phien_dieu_khien and dk_gui_lenh_web. Reject web queue age >=1000 ms and stale SET_MODE/SET_RELAY control revision. Draft commands retain core edit/revision guards. Random boot ID generated in main. HTTP server alone drains web command responses; main no longer consumes them.
- HTTP server owns gateway/socket state. Separate web_task schedules one bounded outstanding work item every 200 ms. Three WS slots, flat commands <=384 bytes, same-Origin check, no authentication/TLS. Slow clients can delay web updates; no measured hard realtime guarantee.
- Wire contract now documented at top of docs/protocol.md (supersedes historical proposal). Dedup is last command per socket, exact JSON bytes, not across reconnect. Save ACK PENDING followed by authoritative snapshot/save_id, not SAVED ACK. UI cancels stale slider edits, reconnects without replay, hides invalid/stale values. docs/web-guide.md explains source and setup.
- Verification: host gateway 142 assertions; network 119 assertions across normal run and 11 injected setup failures; Node UI scenarios pass. Existing seven host suites pass 2820 assertions, total C checks 3081. ESP-IDF 5.5.4 demo and GPIO configurations build successfully (0x57400 and 0x5f3d0 bytes without Wi-Fi credentials). Logs: tests/web/out and tests/control_task/out/firmware*.log. No flash, real radio/socket/browser integration, physical timing or electrical validation.
- Next useful no-board work: end-to-end PC simulator/manual browser exercise using shared protocol; then hardware integration when board arrives. Do not treat board availability as prerequisite for further software development.

## Current authorization and deliverable
- 2026-09-25 LCD implementation: added lcd1602 protocol driver (configurable PCF masks/backlight, 4-bit init/data, fixed ASCII 2x16 frames), ESP-IDF I2C adapter (GPIO21/22, 100 kHz, 20 ms transactions, retain bus/device for retry), display formatter/task (snapshot-only, 200 ms polling, skip unchanged, 1 s retry after errors). On transfer failure reinitialize before next frame. Hardware address 0x27 and map 0x76543210 provisional/configurable, not verified. No hotplug detection while content unchanged; no board prerequisite.
- Snapshot gains owner-maintained loi_luu so LCD preserves Save failure while samples arrive. Display shows independent sensor status and edit/save state; stale snapshot >=1 s shows MAT DONGBO/R:?. R indicates commanded output, not contact feedback. Demo prints LCD frames to serial without I2C. Guide docs/display-guide.md. Host tests pass: display 1948 assertions (including per-byte transport checks), storage 140, runtime 327, sensor 234, core 123, debounce 20, GPIO 28 (2820 total). ESP-IDF 5.5.4 builds passed: demo 0x31eb0 and hardware 0x39be0; firmware.log/firmware_gpio.log in tests/control_task/out. No flash. Next module Wi-Fi/web LAN.
- 2026-09-25 sensor implementation: added mq2 ADC1 channel 6/GPIO34 oneshot driver (12 bit, 12 dB, no calibration), pure C rolling mean up to four samples, sensor_task with timestamp before ADC call, sequence per attempt, full-queue drop/reset and explicit invalid events on read errors, rails 0/4095, read duration or inter-read gap >= freshness deadline. This rail policy is a provisional quality check, not disconnection diagnosis. Raw ADC units remain; no ppm claims.
- main now starts sensor worker in both modes: demo callback low/high/timeout/recovery, hardware ADC callback. Removed old demo Manual/ON commands; Auto/OFF startup retained. NVS remains real; demo does not auto-save/override saved config. CONFIG_MQ2_ON_DINH_GIAY defaults 0 (unknown); hardware warmup UINT64_MAX inhibits Ready while still sampling. 100 ms polling, 500 ms freshness, 3 samples and thresholds are provisional pending hardware work. No board required to continue software.
- Sensor host tests passed 234 assertions, existing suites passed 634 (868 total). Tests cover driver config/errors/cleanup, rolling filter, Auto/Manual fault/recovery, queue-full loss, stale samples, slow reads and unset warmup. Guide: docs/sensor-guide.md. ESP-IDF 5.5.4 builds passed: demo 0x2e370, GPIO/ADC 0x31a10; logs in tests/control_task/out/firmware.log and firmware_gpio.log. sdkconfig now includes default MQ2_ON_DINH_GIAY=0. No flash/hardware validation. Next module LCD1602/PCF8574 + display_task.
- NVS verification update: 634 total host assertions passed (storage 136, runtime 327, core 123, debounce 20, GPIO 28). Both ESP-IDF 5.5.4 builds succeeded with NVS linked: demo 0x2de70, GPIO 0x2fa80. Logs: tests/control_task/out/firmware.log and firmware_gpio.log. No flash.
- 2026-09-25 NVS implementation: added components/storage (single u64 magic/version/threshold key, bounds checking, explicit init/read/set/commit errors, no automatic erase) and app_tasks/storage_task (one owner, retain response on full result queue, no repeated flash write). main loads validated threshold before control startup and launches storage worker; removed temporary always-fail save consumer. NVS is real in both firmware modes; demo GPIO remains fake and sends no save. Init failure inhibits persistence until reset, Manual remains available. Error does not promise on-flash rollback; next boot rereads actual valid record.
- New host storage integration suite passed 136 assertions, compiling real storage/worker/control/core with fake NVS and FreeRTOS; includes response backpressure, stale boot/save IDs, delayed success after timeout, commit failure/retry and restored startup Auto/OFF. docs/storage-guide.md explains code and limitations. No board/flash/power-loss validation. Next module: MQ-2/sensor task; no board prerequisite.
- 2026-09-25: User has no board and wants to implement one module at a time before hardware integration. Board button/relay trial is NOT a prerequisite for further coding. Continue module-by-module with deterministic host tests/fake hardware APIs, ESP-IDF compilation and Vietnamese source explanations; defer electrical measurements, physical timing, calibration and flash until hardware is available. Next module remains NVS/storage_task, followed by MQ-2/sensor, LCD/display and network/web. Passing host tests/build does not establish on-board correctness.
- Final GPIO validation: ESP-IDF 5.5.4 builds succeeded for both demo (`build_control_task`) and real-GPIO selection (`build_gpio`, separate sdkconfig); no flash. Evidence: tests/control_task/out/firmware.log and firmware_gpio.log. `nguon=nut` log reports result, draft and effective threshold. Next implementation is NVS/storage_task after board button/relay trial.
- 2026-09-24 newest continuation: implemented active-high relay GPIO23, active-low buttons GPIO25/26/27/32, pure C debounce (30 ms, no hold repeat, boot-held suppression), polling button_task (10 ms target), and owner-side relative button translation. Queue-full button events are logged/dropped; no retry, no web response credits. Simultaneous edges use ON/OFF/MODE/EDIT order; threshold step 10 clamps at configured bounds.
- app_main keeps default no-GPIO demo; disabling CONFIG_DK_THU_NGHIEM enables button/relay trial, startup Auto/OFF, no sensor samples or valid persisted configuration. Temporary save consumer explicitly returns failure (NVS not implemented). Added docs/button-task-guide.md for source reading/board trial. Physical G0/flash remain unperformed.
- Host evidence for this continuation: core 123, runtime 327, debounce 20, GPIO driver 28 assertions passed. Real IDF 5.5.4 demo build passed after running outside sandbox (sandbox Git ownership blocked external toolchain). GPIO-mode build validation recorded below when completed. This supersedes older claims of no implemented drivers and no IDF build.
- LATEST IMPLEMENTATION: user requested control_task code and explanation. Implemented actual FreeRTOS adapter in app_tasks/control_task, queue/notification loop, response-capacity reservation, separate storage result channel, independent LCD/web snapshots, output callback errors and structured logs. Public producer APIs are nonblocking, task-only; state is private.
- Added app_tasks CMake and app_main simulation guarded by CONFIG_DK_THU_NGHIEM (default y); no real GPIO/ADC/NVS/network. App demo starts service and sends MANUAL/ON commands, not production sensor configuration. Five other tasks and drivers remain unimplemented.
- Host test compiles actual adapter against deterministic fake FreeRTOS: 243 checks passed; core regression 123 checks passed. No SMP/on-board scheduling test.
- ESP-IDF 5.5.4 installation found at D:/SotaTek/AI_robot/ESP32_ENV/Espressif/frameworks/esp-idf-v5.5.4. Three environment setup attempts failed before compiler (PowerShell policy, initializer Python=null, native progress stderr interpreted as terminating error). Stopped retrying environment; build_firmware.ps1 stderr handling corrected but not rerun. Firmware build/flash remain unverified.
- Explanation in docs/control-task-guide.md. Next: validate real IDF build environment, add producer routing and drivers; physical G0 remains pending. No repeat permission for implementation already authorized.
- Latest request: translate implemented code into Vietnamese. Renamed project-defined C types, enums, functions, fields, local variables and test functions to unaccented Vietnamese (dk_ prefix); comments use accented Vietnamese; structured log keys use unaccented Vietnamese. PowerShell runner uses Vietnamese variable names/messages. Existing file/folder paths and standard C/ESP-IDF/PowerShell APIs remain unchanged.
- Updated code-walkthrough.md and codebase-map.md symbol references. Rebuilt/reran host suite after renaming: DAT nhom=10 so_phep_kiem_tra=123. No behavioral changes or ESP-IDF/hardware validation.
- LATEST: User approved P1/P2/P3/P4 = A/A/A/A and explicitly requested implementation plus explanation. This supersedes previous no-code/open-choice notes.
- Implemented Gas_System/components/control_core (pure C state machine/header and ESP-IDF component CMake) and Gas_System/tests/control_core (deterministic host C tests, PowerShell runner). GCC host run passed 10 suites / 123 checks, with -Wall -Wextra -Werror -pedantic. Evidence: tests/control_core/out/build.log and test.log.
- Core covers startup/readiness/hysteresis, Auto/Manual including missing config, sample freshness/sequence gaps, shared edit revision/session, Save request/result/failure/uncertainty and monotonic deadlines. Structured dk_ket_qua is logged by host harness; runtime logging still to integrate.
- Added docs/code-walkthrough.md explaining actual functions, full intended project flow and remaining work. spec.md and test-matrix.md updated. main/app_main is still empty; tasks/drivers/web/NVS/filtering/transport dedup are NOT implemented. No ESP-IDF build, flash or hardware test performed.
- Next implementation: control_task/queue/snapshot adapter and driver integration, respecting outstanding physical G0 checks. Do not claim full G2/integration or on-board completion from host tests.
- User requested continuation after wiring; completed G1 documentation draft: spec.md, state-model.md, api-contracts.md, protocol.md, test-matrix.md. No firmware or tests executed; G0 measurements remain pending.
- G1 defines driver responsibilities, task state ownership, event/deadline handling, draft/version protocol, storage result reconciliation and 19 test scenarios. Architectural API names are conceptual, not compiled headers.
- P1–P4 were open during the G1 draft; user subsequently approved all A choices (see latest status above). Physical parameters still require measurement/configuration.
- User authorized continuing the wiring design. Created docs/wiring.md with complete net connections, GPIO mapping, BOM, supply arrangement (ESP32 USB; external regulated 5 V for modules; common GND, no positive supply tie), and physical acceptance steps.
- Design choices pending measurement: AO divider 22k/10k 1% + 100nF (5V→1.5625V; assumed 5.25V/1% worst case≈1.663V), LCD 5V through bidirectional I2C translator, four 10k button pull-ups, 10k relay pull-down, 1k LED resistor. Documented AO loading and RC limitations.
- Wiring documentation completed; G0 physical checks and toolchain build/flash remain pending. No firmware implemented. Next: physical measurements and bring-up, then G1 contracts (documentation can proceed while measurements are pending).
- User requested GPIO recommendations to avoid interference/non-operation. Proposed map documented in hardware.md: MQ-2 AO GPIO34/ADC1_CH6, LCD SDA21/SCL22, buttons 25/26/27/32, active-high relay23; DO unused. No pin configuration code or physical validation performed.
- Pin choice separates ADC1/Wi-Fi compatibility from electrical noise. Documented analog level conditioning, 100 nF ADC filtering proposal, I2C voltage translation when backpack is powered at 5 V, and boot/flash/UART restrictions.
- User supplied LCD photo consistent with LCD1602 and a four-pin I2C backpack with contrast potentiometer. IC marking/address cannot be read reliably; PCF8574 remains user-confirmed, not visually verified. No new firmware or wiring decisions from this image.
- Latest user correction: LCD1602 uses a PCF8574 backpack. Earlier Waveshare RGB identification/PDF was a mistake and must not determine driver, addresses or supply. I2C address, backpack mapping and supply/pull-ups remain unverified. MQ-2 photo shows FC-22 with VCC/GND/DO/AO.
- User explicitly limits relay work to GPIO HIGH/LOW. Do not request relay model/electrical details again or block software planning on them. This supersedes earlier relay verification gates; no electrical validation is claimed.
- Hardware update: user confirmed I2C-backpack LCD1602, MQ-2 AO/DO module, discrete LED with resistor supplied by 3.3 V, and active-high relay (HIGH on, LOW off). Relay input compatibility with ESP32 3.3 V remains unverified; module supply remains specified as 5 V.
- Added docs/hardware.md for G0. sdkconfig identifies ESP-IDF 5.5.4 as its generating version; idf.py is not found in the current shell, so the active toolchain version is not verified.
- Latest user request: separate folders for each task and one drivers folder for module communication, plus explanation of the next step. Interpreted tasks as FreeRTOS application tasks in the ongoing RTOS discussion.
- Created README-only folders under Gas_System/components/app_tasks/{control_task,sensor_task,button_task,display_task,storage_task,web_task} and Gas_System/components/drivers/{mq2,lcd1602,buttons,relay}. Updated project-plan.md layout. No C sources, public APIs, CMake registration or running tasks added.
- Next step remains G0: identify LCD interface, MQ-2/module variants, relay input polarity/voltage compatibility, LED supply and ESP-IDF version before selecting pins and writing drivers. Task counts and runtime parameters remain proposed until G1.
- User explicitly requested a phased plan with outputs and completion criteria after the RTOS discussion.
- Planning is now authorized; this supersedes earlier discussion-only/no-plan notes below. Firmware implementation has not been requested in this turn.
- Created `docs/project-plan.md`: G0 hardware/environment validation, G1 behavior/protocol specification, G2 deterministic control tests, G3 local hardware, G4 persistence, G5 web synchronization, G6 integration/robustness, G7 portfolio.
- Architectural choices in the plan are recommendations, not previously user-approved requirements. Open behavior conflicts are assigned to G0/G1 rather than silently resolved.
- Initial measured-performance targets in the plan are provisional, not achieved results. No firmware, scaffold, build or test execution was performed.

## Confirmed user intent
- Discuss an MQ-2 embedded portfolio project for Fresher Embedded applications.
- Hardware: LCD 16x2, four buttons, relay control, Auto/Manual, editable threshold.
- Web: measured value, relay control, Auto/Manual, threshold slider; bidirectional live synchronization including threshold editing state.
- Threshold button enters editing; pressing again saves and exits.
- Discuss improvements, scope reductions, RTOS and event-driven concepts before planning.
- Do not create an implementation plan or write firmware until the user explicitly agrees.
- User confirmed ESP32-DevKit V1 with ESP-IDF, one relay module described as 5 V / 10 A, and web access on the same Wi-Fi network.
- User has used RTOS and wants deeper design and testing experience.
- User confirmed the relay switches a discrete LED/resistor demo load supplied by 3.3 V; relay is active-high. Exact module electrical interface remains unverified.

## Observed workspace
- Gas_System is an ESP-IDF C skeleton targeting esp32 in sdkconfig.
- app_main was empty at the initial inspection. Board is now user-confirmed; exact peripheral module variants and wiring remain unverified.

## Confirmed behavior decisions
- Manual: relay follows hardware/web commands; LCD and web still indicate threshold exceedance.
- Threshold editing: hardware and web share one draft; ESP32 processes changes sequentially. Stale-version handling is required; exact protocol remains undecided.
- Startup/reset: retain the saved threshold, start in Auto, and keep relay off until sensor readings are declared ready.
- Readiness: elapsed stabilization time grounded in documentation/experiments AND a sequence of fresh, successfully read samples passing validity checks. Timing, sample count and validity criteria remain to be specified.
- Web disconnect during editing: retain shared draft on ESP32; hardware can continue editing/saving. Cancel draft after inactivity from both interfaces; duration remains unspecified.
- Invalid/missing sensor data in Auto: turn relay off and show a distinct error, never interpret this as gas-safe.
- Auto to Manual: retain current relay state, then await manual commands.
- Manual to Auto: immediately evaluate valid readings against the saved threshold; relay off if sensor is not ready.
- Runtime sensor recovery: require a sequence of valid samples before Ready; Auto immediately reevaluates the output.
- Manual during sensor fault: permit LED relay control while LCD/web continue showing the sensor fault.
- Missing/invalid persisted threshold: relay off, show configuration-required status, and inhibit Auto control until a valid threshold is saved.
- Threshold save failure: report error, retain the previous effective threshold and keep the draft for retry.
- User accepted these supplementary behaviors as requirements for continued discussion, not authorization to create a plan.

## Discussion proposals, NOT approved decisions
- Current user-selected topic: RTOS and web/hardware synchronization, data ownership, queues, races and stale commands. This authorizes conceptual explanation only.
- Teaching proposals: a single control-state writer, copied snapshots to UI consumers, bounded command queues, explicit command outcomes, scoped draft revisions and edit/boot session identity. Queue serialization alone does not prevent semantic stale writes.
- Distinguish relative button increments from absolute slider writes; validate Save against the expected draft revision; correlate asynchronous persistence results with the submitted draft.
- Discuss backpressure, no silent command drops, sensor freshness deadlines, and deterministic event-sequence tests. None of these mechanisms is approved implementation architecture yet.
- User selected conceptual discussion of state machines, event-driven behavior and conflict priorities; no implementation planning authorization.
- Teaching model: separate mode, sensor health, edit lifecycle, configuration validity and connection state; relay command is resolved from those states. This is illustrative, not approved architecture.
- Explain guards, transitions, effects, sequential event processing, data freshness, stale draft revisions, and save-result handling. Semantic priority is distinct from RTOS task priority.
- Remaining conflict-policy examples: mode changes during editing, edits during pending save, expiry versus late commands, and Manual with missing configuration. Do not silently settle these as requirements.
- Device-authoritative state is proposed; shared draft editing is confirmed above.
- Context-sensitive ON/+ and OFF/- buttons; Auto/Manual separate from editing state.
- Consider FreeRTOS, event queues, state machines, debounce, filtering, hysteresis, persistence, reconnect handling and deterministic tests.
- Local web scope and one relay are confirmed; deferring cloud and OTA remains a proposal.
- Avoid claiming calibrated ppm without a defined calibration method.

## Open questions
- LED resistor/value and forward voltage, LCD on-device address verification, MQ-2 AO voltage/interface, and time budget. Relay scope is GPIO only per user.
- Readiness parameters, stale-data timeout, edit inactivity timeout, measurement units and Auto hysteresis values.
- Detailed UI button mapping and behavior during editing remain proposals; combined-edge-case priorities need specification later.
- Next discussion: single ownership of control state, explicit SET commands, acknowledgments and revisions, bounded queue behavior, and deterministic tests with injected sensor/time inputs. These are proposals, not approved architecture.

Planning and context documentation were added; no firmware changes or tests were performed.


## Flame sensor D0 addition (2026-10-20)

- Added configurable digital D0 input, default GPIO4, and configurable active-low polarity (default active-low). Driver is `components/drivers/flame_d0`; `sensor_task` samples it on its existing 100 ms cycle and logs only initial state/transitions or read errors.
- Flame status is monitor-only: it does not affect gas control, relay, web, or Telegram. Compile-time checks reject unsupported pins and collisions with configured pins.
- Wiring documentation requires common ground and D0 <= 3.3 V recommended; verify actual module output before connecting. No physical board validation has been performed.

## Flame D0 test-file cleanup (2026-10-20)

- Removed the standalone `components/drivers/flame_d0/test/test_flame_d0_logic.c` at the user request. It was only a manual host test, not part of firmware CMake. Production polarity conversion remains in `flame_d0_logic.h`; no firmware behavior changed.


## Auto relay response to flame sensor (2026-10-20; approved behavior, implementation pending)

- In Auto, relay demand is the OR of the existing MQ-2 hysteresis state and flame D0 detection. Flame detection keeps relay ON even when MQ-2 is below threshold; relay turns OFF only when there is no flame and MQ-2 has reached the existing OFF boundary (threshold minus 50 ADC).
- In Manual, relay remains controlled by button/web commands; user may turn it OFF even while D0 reports fire.
- When flame clears, Auto resumes MQ-2 hysteresis evaluation; Manual retains its current relay state. D0 read error is logged and treated as no active flame so the existing MQ-2 policy continues.
- User selected A: in Auto, valid flame detection may turn relay ON even while MQ-2 is warming, invalid or has no saved threshold. D0 read errors remain treated as no active flame; in that case the existing MQ-2 readiness/configuration gates still apply.


## Flame Telegram and configurable hysteresis decisions (2026-10-20; implementation pending)

- User selected Telegram alerts on both flame detection and flame-clear transitions, in Auto and Manual. Preserve the existing Telegram delivery policy: one attempt; offline, busy, or failed sends are logged/dropped without retry or deferred delivery.
- If gas and flame notification transitions are produced by the same control processing event, combine them into one Telegram message. D0 transition is immediate at the existing sampling rate; no consecutive-sample debounce.
- Add one menuconfig hysteresis setting in raw ADC counts, default 50 and proposed valid range 1..100 (bounded by current minimum gas threshold 100). The shared value controls Auto gas relay OFF boundary and gas Telegram recovery boundary; flame D0 uses no ADC hysteresis.
- No firmware code has been changed for Telegram flame notifications or menuconfig hysteresis. User has approved the requirements/plan choices, but has not yet instructed implementation to begin.

## GPIO interrupt direction (2026-10-22)

- User selected staged option C: first add GPIO interrupt handling for the four active-low buttons, preserving task-level 30 ms debounce and existing control-task ownership; after that is verified, add a separate D0 flame GPIO interrupt implementation.
- MQ-2 AO remains periodic ADC sampling. Relay, LCD, web and Telegram must never execute in a GPIO ISR. ISR scope is limited to wake/queue notification using ISR-safe FreeRTOS APIs; the receiving task validates the physical signal then forwards the normal event to control_task.
- No source implementation is authorized by this decision alone; discussion/plan stage continues until the user requests code.

## GPIO interrupt implementation (2026-10-22)

- Implemented the user-selected staged scope in one firmware change: the four active-low buttons use GPIO negative-edge interrupts, and the flame D0 input uses GPIO any-edge interrupts. Both ISRs do only `vTaskNotifyGiveFromISR()` and never read GPIO, log, access shared control state, change relay state, display LCD, publish web state or send Telegram.
- `button_task` is awakened early by a button interrupt but still polls on a 10 ms timeout and uses the same 30 ms stable debounce rule before calling `control_send_button`. `sensor_task` runs at its existing 100 ms cadence without D0 edges; on a D0 interrupt it waits 30 ms for the D0 output to settle, then executes its normal `sensor_run_once` path. Therefore control_task remains sole state owner and the existing 3-second fire-clear policy is unchanged.
- The ESP-IDF GPIO ISR service is shared safely: whichever driver starts first installs it; the other accepts `ESP_ERR_INVALID_STATE` as already installed and adds its own per-GPIO handler. Failure while adding button handlers rolls back only handlers added by the button driver.
- Added `button_debounce.h` plus standalone GCC test `test_button_debounce.c`. It passes with `-std=c11 -Wall -Wextra -Werror -pedantic`, verifying short bounce suppression and exactly one event while a button is held. ESP-IDF 5.5.4 Ninja build passed: `Gas_System.bin` 0x1051d0 in 0x177000 app partition, 0x71e30 (30%) free. Build logs: `%TEMP%\gas_interrupt_ninja_final.log` and `%TEMP%\gas_interrupt_ninja_after_test.log`. No board flash or physical interrupt/noise measurement has been performed.
