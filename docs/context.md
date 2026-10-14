# Project context

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
