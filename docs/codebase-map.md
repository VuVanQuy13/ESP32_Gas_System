# Codebase map

## Telegram bot notification integration (2026-10-17)

- Current source boundary: `main/Gas_System.c` starts the control service and later starts `network_start()` and `web_start()`. `components/app_tasks/control_task/control_task.c` serializes button, web and sensor events. `components/control_core/control_core.c` owns the pure state machine and Auto relay hysteresis. Wi-Fi `IP_EVENT_STA_GOT_IP` only confirms a local IP address, not Telegram reachability.
- Completed parts 1 and 2: local `sdkconfig` files are ignored/untracked while `sdkconfig.defaults` keeps non-secret defaults; the Kconfig menu provides Telegram enable/token/chat ID. Secrets are not printed; a compiled bot token remains extractable from firmware.
- Completed part 3: `components/telegram_notify` provides `telegram_notify_start()` and nonblocking `telegram_notify_submit()`. Its single worker performs one HTTPS POST using ESP-IDF certificate verification, checks HTTP status and Telegram JSON `ok`, then drops the event. A one-event gate drops submissions while an event is queued or sending. Logs contain event kind/raw ADC/threshold/result, not token, chat ID, URL or request body. Native message-format tests previously passed, but their source was removed at the user's request on 2026-10-19. `main/CMakeLists.txt` links the component, and enabling Telegram selects the TLS certificate bundle.
- Part 4 complete: `components/control_core` tracks a latched gas alarm and emits `CONTROL_NOTIFICATION_GAS_ALARM` / `CONTROL_NOTIFICATION_GAS_RECOVERY` in `control_result`, independent of Auto/Manual and relay output. Recovery requires three valid samples <= threshold - hysteresis; invalid/above-boundary readings reset progress. Draft threshold edits do not affect policy.
- Part 5 complete: `main/Gas_System.c` starts the Telegram worker when enabled and records success in `control_task_config.telegram_notifications_ready`. `control_task.c` maps core transition values into `telegram_notify_event` and invokes `telegram_notify_submit()`, which is nonblocking. If Telegram is disabled, startup credentials are invalid, the queue is busy, or delivery fails, the notification is skipped/dropped according to the existing single-attempt policy; relay/control processing continues.
- Runtime flow: MQ-2 sample -> control owner detects alarm/recovery -> nonblocking submit -> notification worker attempts one HTTPS request -> result log. Busy, offline, transport, HTTP, API or queue-admission failures are dropped without retry/deferred replay. Physical-board Wi-Fi and actual Bot API delivery remain unverified.
- Requirements: alert on valid reading >= effective threshold; recovery after 3 consecutive valid readings <= effective threshold - 50 ADC; reset recovery count on invalid/above-boundary readings; do not Telegram-notify sensor faults. Draft threshold edits do not affect alerts until saved; a committed threshold change may raise an alarm if the current ready sample exceeds the new threshold. Recovery debounce still requires three new valid readings. Message labels the measurement as raw ADC, and the bot recipient must start/contact the bot first.

## Telegram setup guide

- `docs/telegram-guide.md` documents configuration and deployment. It matches the current chat ID validator: numeric private IDs and negative numeric group IDs are accepted; channel usernames such as `@name` are not.
- The standalone Telegram-format and control-core notification test source files were removed on 2026-10-19; component CMake source lists never included them, so firmware composition is unchanged. Automated regression checks for these cases are no longer available in the tree.

## Opaque handle type aliases (2026-10-13)

- `control_task.h` declares `typedef struct control_service control_service_t;`; `struct control_service` is the tag, and `control_service_t` is a typedef alias for the same struct type. Callers use `control_service_t *` for a pointer; the struct body remains private to `control_task.c`.
- The same naming convention applies to `sensor_task_t`, `display_task_t`, and `storage_task_t`. These are opaque handles; their task state remains private in their corresponding `.c` files. This is a naming-only change with no runtime behavior change.

## `control_task_start` initialization note (2026-10-11)

- `control_task.h` forward-declares opaque `control_service_t`; `control_task.c` owns the `struct control_service` definition and heap allocation. `control_task_start(control_service_t **out, ...)` receives the address of the caller's handle so it can return the created service through `*out`, while its `esp_err_t` return reports success/failure separately.
- Startup order: validate pointers/callback -> clear `*out` -> `calloc` and shallow-copy config -> initialize control core -> allocate 6 queues + response counting semaphore -> request relay OFF -> log and publish initial snapshots -> create owner task -> assign `*out`. Every failure returns an ESP error and releases allocated resources. Caller must pass a valid writable `control_service_t *` variable; callback context lifetime must cover service lifetime.
- No source edit or validation was performed for this explanation.

## Web address and source encoding (2026-10-07)

- On DHCP success, `components/network/network.c` logs the access URL. `components/web_service/web_task/web_task.c` logs the actual HTTP port (normally 80). Browse to the logged URL; no static IP is configured. Checked-in Wi-Fi credentials are empty and no ignored `wifi_local.h` is present in the audited workspace, so Wi-Fi configuration is required before the board can publish an IP.
- Firmware comments are UTF-8 Vietnamese and identifiers remain English. Browser assets remain Vietnamese. All 33 application C/header files were checked for invalid UTF-8 and obvious mojibake markers; none remain after comment repair.

## Embedded naming convention (2026-10-01)

- Firmware C identifiers use English: `control_*`, `sensor_*`, `button_*`, `storage_*`, `display_*`, `network_*`, `relay_*`, `mq2_*` and `lcd_*`. Vietnamese comments describe intent for a beginner. Browser assets retain Vietnamese UI text and their existing wire protocol field names to preserve compatibility.

## Current runtime flow (2026-10-01)

- `main/Gas_System.c` creates the real hardware setup and starts `control_task`, then storage, sensor, display and button tasks. It starts Wi-Fi and web service last.
- `control_task` is the sole mutable owner of `dk_trang_thai`; each other task communicates through its APIs/queues or consumes its LCD/web snapshots. This serialization is the synchronization boundary between physical buttons and browser commands.
- Sensor samples, debounced button presses, storage completion and web JSON commands become `dk_su_kien`. `control_core` applies each event, while `control_task` performs output effects (relay), sends NVS jobs and publishes snapshots.

## Removed fake runtime (2026-10-01)

- `main/Gas_System.c` is now a hardware-only composition root: its ADC, relay and LCD callbacks call `mq2_doc`, `relay_dat` and `lcd_i2c_xuat` directly. The previous compile-time fake-data branch is removed.
- `main/Kconfig.projbuild`, `sdkconfig` and `sdkconfig.old` no longer contain `DK_THU_NGHIEM`. Build validation passed with ESP-IDF v5.5.4; no board was flashed.

## Main-source teaching walkthrough (2026-09-29)

- `main/Gas_System.c` is the composition root. It owns startup-local configuration and passes copied configuration or pointers to persistent objects into control, storage, sensor, display, network and web modules. See current conversation for a line-linked walkthrough; no source changes.

## Beginner-readable source layout (2026-09-29)

- User authorized formatting all application source: 33 C/header files and app.js, plus HTML/CSS and simple component CMake registrations. Expanded compact statements/functions/enums, designated initializers and startup sections; added Gas_System/.clang-format for baseline 4-space, 100-column formatting. No policy/API/pin/configuration changes intended.
- Verification: C/header non-whitespace content and literal comparison (normalizing adjacent C strings and macro line continuations), JS whitespace comparison + node --check, HTML tag/attribute/nonblank-text comparison, CSS/CMake token comparison passed. Backups/check evidence in TEMP/gas_format_d2zczr1z. Final ESP-IDF 5.5.4 build passed with current sdkconfig (TEMP/gas_lcd_mapping_build.log). No physical-board test.

## Monitor logging audit (2026-09-27)

- Audited log sites in app tasks, drivers, storage, network, web service and main. Added named debounced button events, human-readable mode/sensor/edit/relay state, startup state, successful relay callback, processed web command outcome, threshold classification changes including Manual/unknown, HTTP startup port and Wi-Fi configuration/disconnect diagnostics. Existing IP URL, storage and LCD error/recovery logs retained. No credentials/payloads logged.
- Sensor INFO logs now first sample/every 1000 ms/validity-or-delivery transition; normal sample control logs suppressed. Sampling and control decisions unchanged. Current sdkconfig default/max log level is INFO. Added monitor guide to main/interview.md.
- Host GCC checks passed for log cadence/fault/recovery/queue-error and threshold boundaries/Manual/unknown. Final ESP-IDF 5.5.4 build passed with current sdkconfig; log TEMP/gas_lcd_mapping_build.log. No board test.

## LCD mapping menu simplification (2026-09-27)

- Removed LCD_SO_DO from main/Kconfig.projbuild at user request. Main now calls existing driver lcd_so_do_mac_dinh(), replacing hex decoding. Mapping stays P0=RS, P1=RW, P2=E, P3=BL, P4..P7=D4..D7; backlight polarity option preserved with explicit true/false branches. SDA/SCL and address options unchanged. Custom old LCD_SO_DO settings no longer apply.
- Updated driver comments and main/interview.md. Host GCC assertions verified default masks match the former 0x76543210 mapping; IDF 5.5.4 build passed with current sdkconfig (log in TEMP/gas_lcd_mapping_build.log). No board validation.

## Current operating map — source read 2026-09-27

This section describes current application sources and supersedes historical implementation/tooling statements below. Scope: all application .c/.h, web assets, project/component CMake and Kconfig; selected sdkconfig settings. Generated build files and external ESP-IDF implementation are not application source reviewed here. No build/test/board execution in this walkthrough.

### Modules and edit surfaces

All paths below are relative to Gas_System/.

| Module | Source and responsibility | Dependencies / consumers |
| --- | --- | --- |
| Startup | main/Gas_System.c: callback selection, boot ID, hardware init, NVS restore, task startup, network/web | All application components |
| Policy | components/control_core/control_core.c and include/control_core.h: pure C state machine, Auto/Manual, readiness, hysteresis, editing, save deadlines | Called only by control_task at runtime |
| State owner | components/app_tasks/control_task/control_task.c/.h: private mutable state, input/response queues, relay callback, save jobs, independent snapshots | Core, producers, storage, LCD/web |
| Sampling | components/app_tasks/sensor_task/sensor_task.c/.h: read callback, mean of up to 4 samples, validity, timestamp and sequence | MQ-2 driver -> control input |
| Buttons | components/app_tasks/button_task/button_task.c/.h: 10 ms polling, 30 ms debounce, press edges | GPIO buttons -> owner-relative commands |
| Persistence worker | components/app_tasks/storage_task/storage_task.c/.h: NVS job execution, retained response on queue backpressure | Owner save queue -> storage -> result queue |
| Display | components/app_tasks/display_task/display_task.c/.h: snapshot formatting, changed-frame output, error retry | Owner LCD mailbox -> LCD callback |
| Hardware | components/drivers: board_pins.h, mq2, buttons, relay, lcd1602 protocol and I2C adapter | ESP-IDF ADC/GPIO/I2C; task callbacks |
| NVS | components/storage/storage.c and include/storage.h: validated u64 record, namespace gas_cfg/key nguong_v1, set+commit | Startup read; storage worker write |
| Network | components/network/network.c: Wi-Fi STA, IP logs, 5 s retry timer | ESP-IDF Wi-Fi/event/netif; optional wifi_local.h overrides Kconfig credentials |
| Web | components/web_service/web_gateway.c, web_task/web_task.c, include headers and web assets | HTTP owns sockets/gateway; commands -> owner; snapshots/results -> browsers |

### Runtime edges and invariants

- app_main -> optional physical driver init -> NVS init/read -> control -> storage -> sensor -> display -> physical buttons -> network -> HTTP. Tasks can run as soon as created; app_main does not contain the application loop.
- MQ-2 AO -> ADC1 12-bit/12 dB -> sensor mean filter -> input queue -> core -> relay GPIO and snapshots. Raw ADC units, no ppm calibration. Invalid rails 0/4095, slow reads or gaps >=500 ms reset filter; queue-full sample drops preserve sequence gaps.
- Control input and command response queues each have 16 slots; counting semaphore reserves response capacity before accepting commands. Samples/buttons have no command ACK. LCD/web each use an independent overwrite mailbox of length 1. Save job/result queues each have length 1.
- Each control turn checks deadlines, handles at most one save result and one input, then publishes snapshots. Continuous work yields one tick; idle wait follows next deadline capped at 100 ms. Current FreeRTOS 100 Hz implies 10 ms ticks, not measured hard-real-time latency.
- Ready requires warmup elapsed plus 3 consecutive valid samples. Auto needs Ready and valid persisted threshold; ON at sample >= threshold, OFF at sample <= threshold-50, otherwise holds. Manual allows explicit ON/OFF and does not force OFF on sensor failure. Startup is Auto/OFF.
- Edit -> shared draft -> save job -> nvs_set_u64 + nvs_commit -> correlated result -> promote draft to effective threshold. Old threshold remains in use during editing/saving. Edit inactivity timeout 30 s; save timeout 2 s marks unknown and continues waiting. Storage retries delivery without rewriting NVS.
- Buttons are active-low pull-up inputs; ON/+ and OFF/- use step 10 while editing, otherwise request relay. MODE toggles; EDIT begins edit or saves. Held-at-boot press suppressed; no hold autorepeat.
- LCD consumes state every 200 ms, pads two ASCII rows to 16 characters, skips identical frames; errors retry after 1 s. Protocol sends 4-bit HD44780 commands through PCF bytes; adapter uses I2C 100 kHz and 20 ms transaction timeout. Auto-address mode requires exactly one responder among 0x20..0x27/0x38..0x3f.
- Web helper posts at most one pending HTTP work item every 200 ms. Three WebSocket clients, commands <=384 bytes. Schema/boot/edit/revision/control checks protect state; owner rejects web queue age >=1 s. Dedup stores last exact JSON command per socket. QUEUED is admission, APPLIED is processed, PENDING is save in progress; final save outcome comes from state. Browser reconnects without replay.

### Configuration, build and validation

- main/Kconfig.projbuild defines all project settings -> sdkconfig -> generated sdkconfig.h. board_pins.h performs compile-time pin admissibility/collision checks and maps ADC1 channel.
- Current saved sdkconfig: ESP32, DK_THU_NGHIEM=y, MQ2_ON_DINH_GIAY=0, GAS_WEB=y, LCD_TU_TIM off, LCD_DIA_CHI=0x27, LCD_SO_DO=0x76543210. Pins: MQ2 34, relay 23, SDA/SCL 21/22, buttons 25/26/27/32.
- Demo injects low 400/high 900/read-error/recovery samples over a 15 s cycle and logs relay/LCD; NVS remains real. Hardware warmup 0 deliberately prevents Ready. Initial draft 600 does not enable Auto until successfully saved.
- Native ESP-IDF workflow from Gas_System/: idf.py menuconfig, idf.py build, idf.py -p COMx flash monitor. Use the actual board port. For physical I/O disable demo and configure warmup/LCD/GPIO; Wi-Fi credentials can come from menuconfig or private header. No deployment executed here.
- Web assets are embedded by components/web_service/CMakeLists.txt; no external frontend server or Node runtime on ESP32. tools/tests contain no current runners; old documented scripts cannot be invoked from this inventory.
- Change policy in control_core; wiring in Kconfig/board_pins; timing/filtering in corresponding task; schema changes require gateway and app.js consistency. Verify with a firmware build and relevant deterministic tests when available, then board acceptance. Historical pass counts below are not current reruns.

### Known limits

- Actual electrical behavior, MQ-2 calibration/warmup, LCD backpack mapping and on-board scheduling remain unverified in this walkthrough. Relay status is commanded state, not measured contact feedback.
- app.js can say below threshold when config is absent but sensor Ready; unchanged LCD frames skip bus access, so LCD disconnection/power-cycle detection is not guaranteed.
- LAN HTTP/WebSocket has same-Origin checking but no authentication/TLS. Wi-Fi startup is still attempted when configured even if GAS_WEB is off. Local control has no dependency on successful Wi-Fi startup.

## Historical records

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


## Current inventory clarification (2026-09-27)

- At latest file-explanation request, tools/ and tests/ directories exist but are empty. Their previously described implementations and test results below are historical; do not claim current runners exist. Some guides still point to removed tools. Source build components remain present.
- File roles: `.c` implements behavior; public `.h` declares shared types/APIs; `*_noi_bo.h` exposes task step functions primarily for internal/test use. Task-local `nut_chong_rung`, `cam_bien_loc`, `hien_thi_khung` can be folded into corresponding task files with include/CMake updates if requested. Keep core policy separate from RTOS state owner, and hardware drivers separate from task scheduling.
- Only explanation provided; no source consolidation or deletion authorized by the current request and none performed.

## Review observations (2026-09-27)

- `docs/review-2026-09-27.md` records open issues: web missing-threshold classification and display unchanged-frame fault detection/recovery. Reproductions saved in ignored tests/web/out and tests/display/out. Production sources unchanged during review; host suite passes but lacks these expected-correct-behavior assertions.
- Firmware CMake excludes tests/tools; tests is development-only, tools is the current PC configuration/build/flash workflow. Removing tools requires manual equivalent SDKCONFIG/Wi-Fi setup; removing tests loses verification but not device runtime. User asked consequences, no deletion performed.

## Latest configurable GPIO (2026-09-27)

- `drivers/Kconfig.projbuild` -> generated `sdkconfig.h` -> `drivers/board_pins.h` -> GPIO/I2C/ADC drivers. Eight GAS_GPIO_* settings; compile-time admissibility/collision guards and ADC1 channel mapping. Main startup logs actual configured pins.
- `tools/firmware.ps1 -Action Menuconfig` edits personal `config/sdkconfig.board`; this ignored persistent profile is outside build. prepare_firmware preserves GPIO while applying JSON-owned settings. Native idf.py uses source sdkconfig instead; do not mix profiles unintentionally.
- `tests/pins/`: 32 compiler policy cases plus real GPIO, sensor and LCD suites with alternate pin definitions. Host fake sdkconfig supports -D overrides; normal suites retain default pins.
- `docs/gpio-menuconfig.md`: usage and GPIO restrictions. `docs/verification-2026-09-27.md`: observed passing tests/builds retained after requested generated-output cleanup. Source/tests/docs retained; build/cache/out can be regenerated.

## Firmware handoff (latest, 2026-09-26)

- Primary entry: root `README.md` -> `docs/flash-guide.md`, then `docs/wiring.md`. Agreed firmware modules are integrated; PC simulator is optional, not needed before flashing.
- `Gas_System/config/board.example.json`: schema for ignored personal board.local.json (Wi-Fi, warmup, LCD).
- `Gas_System/tools/prepare_firmware.py`: strict config validation, safe C literal generation, separate real-hardware sdkconfig, private generated Wi-Fi header. Writes only dedicated build_ready/build_ready_test directories; leaves source sdkconfig intact.
- `Gas_System/tools/firmware.ps1`: ESP-IDF terminal Build/Flash/Monitor entry; explicit COM required for serial operations. `components/network/CMakeLists.txt` prioritizes selected generated configuration directory.
- `drivers/lcd1602/lcd1602_i2c.c`: optional 16-address PCF scan on existing bus; exactly one ACK required; LCD_TU_TIM set by handoff profile. Existing mapping protocol unchanged. Tests/display adds absence/ambiguity/timeout/success/no-rescan cases.
- `tests/release`: profile validation/generation tests and non-secret build-only board fixture. `tests/run_all.ps1`: unified host runner. `tests/build_firmware.ps1 -ThuBanGiao`: full linked hardware+Wi-Fi/web build in ignored build_ready_test.
- Evidence: 4090 C checks + Node UI scenarios + four Python tests pass; complete IDF 5.5.4 linked firmware 0xd3b20 bytes, 17% app partition free. No flash or RF/electrical validation. All wiring assumptions and unset warmup are explained in flash-guide.

## Latest Wi-Fi/web implementation (2026-09-26)

- `Gas_System/components/network/`: STA events and retry timer. Optional ignored `include/wifi_local.h`; absent credentials skip networking. Failures return to main; setup retry requires reset.
- `Gas_System/components/web_service/web_gateway.c`: strict flat JSON -> owner queue; per-socket last-command dedup, result routing, snapshot JSON. Pure gateway host-tested with real cJSON and fake RTOS.
- `Gas_System/components/web_service/web_task/web_task.c`: HTTP static assets and `/ws`; HTTP task owns gateway/fds, helper task queues bounded periodic work. Three WebSocket slots. The old `app_tasks/web_task` folder points here.
- `Gas_System/components/web_service/web/`: Vietnamese HTML/CSS/JS; authoritative snapshots, four buttons/shared slider, pending/save/reconnect handling. Node VM tests use fake DOM/socket/clock.
- `control_task`: `dk_gui_lenh_web`, receive timestamp and control revision guard; `dk_ban_chup.phien_dieu_khien` separate from telemetry sequence. Existing draft guards remain in core. HTTP owns response consumption, main no longer drains it.
- `Gas_System/main/Gas_System.c`: random boot ID; starts local tasks, then optional network/web. Existing fake/hardware selection retained; NVS real in both.
- `Gas_System/tests/web/`: gateway 142 + network 119 C assertions and Node UI scenarios. Older suites 2820 checks pass. ESP-IDF demo/GPIO builds pass without configured credentials; no board/live browser transport verification.
- Read `docs/web-guide.md` and newest section of `docs/protocol.md` for implemented flow, schema and limitations. Next no-board surface: PC end-to-end simulation/manual browser tests.

## LCD implementation (2026-09-25)

- Verified 2820 total host assertions; ESP-IDF 5.5.4 demo and hardware builds pass (0x31eb0 / 0x39be0 bytes). No board/flash. SDK config includes LCD address/map/backlight defaults.

- `Gas_System/components/drivers/lcd1602`: callback-based PCF8574/HD44780 driver plus ESP-IDF I2C adapter. Address/map/backlight settings in main/Kconfig.projbuild; GPIO21/22, 100 kHz. Unverified hardware defaults.
- `components/app_tasks/display_task` under Gas_System: pure formatter and snapshot consumer; changed-frame writes, retry on LCD error, latest state on retry. LCD failure cannot modify control state.
- `control_task`: owner-maintained loi_luu added to LCD/web snapshot; retained across samples, cleared on new edit/save/success. `main`: starts display with serial demo or real I2C callback.
- `tests/display`: real protocol/adapter/formatter/task/runtime with fake I2C/FreeRTOS; 1948 assertions. Storage now 140 checks. `docs/display-guide.md` explains source, LCD text, configuration and limitations. No physical LCD validation.

## Sensor implementation (2026-09-25)

- Verified both ESP-IDF 5.5.4 variants: demo build 0x2e370 bytes, ADC/GPIO build 0x31a10 bytes. 868 total host assertions pass. Default sdkconfig adds MQ2_ON_DINH_GIAY=0; no flash performed.

- `Gas_System/components/drivers/mq2/mq2.c/h`: ADC1 channel6 GPIO34 oneshot driver, 12 bit/12 dB. Public dependency esp_adc. No conversion to calibrated units.
- `Gas_System/components/app_tasks/sensor_task`: callback acquisition, pure C mean filter, sequence/timestamp/validity events to control_task. Step API in private header supports deterministic host tests.
- `main/Gas_System.c`: starts real/fake sensor depending on DK_THU_NGHIEM. Default hardware warmup unset (MQ2_ON_DINH_GIAY=0) inhibits Ready. Removed old synthetic Manual/ON commands; starts Auto/OFF. LCD/web still absent.
- `tests/sensor`: actual mq2/filter/sensor/runtime/core compiled with fake ADC/FreeRTOS, 234 checks; total host checks 868 with existing suites. `docs/sensor-guide.md` explains source and provisional quality/timing policies. No board tested.

## Storage implementation (2026-09-25)

- Verification: 634 host assertions total passed, including 136 new storage assertions; both ESP-IDF 5.5.4 demo and GPIO builds passed. Hardware remains unavailable; no flash or power-loss test.

- `components/storage` under Gas_System: ESP-IDF component depending on nvs_flash; `storage.c` and `include/storage.h` implement validated u64 threshold record. No automatic erase, migration or hardware calibration.
- `components/app_tasks/storage_task`: polls save jobs, writes via storage, retains correlated result until control queue accepts it. Private step API permits deterministic testing; only worker calls it at runtime.
- `main/Gas_System.c`: initializes/reads NVS before starting control, starts worker, retains only demo response consumer. Both build modes use real NVS. Supersedes historical no-NVS/always-fail descriptions below.
- `tests/storage`: fake NVS APIs plus existing FreeRTOS fakes compile actual storage/worker/runtime/core; 136 checks passed. `docs/storage-guide.md`: source reading order, startup/save flow, errors, host runner, flash limitations.

## Workflow update (2026-09-25)

User has no board. Implement and explain one module at a time using fake hardware APIs for deterministic host testing and real ESP-IDF compilation. Physical validation is deferred, not a gate for further coding. Next module: storage/NVS; current firmware implementation remains as described below.

## Latest GPIO implementation (2026-09-24)

- Both ESP-IDF build variants verified: `build_control_task` (demo) and `build_gpio` (GPIO enabled). Script `tests/build_firmware.ps1 -ThuGpio` uses separate sdkconfig under ignored test output. No flash performed.

- `Gas_System/components/drivers/relay/relay.c`: GPIO23 LOW initialization and active-high output, called by control owner after startup.
- `Gas_System/components/drivers/buttons/buttons.c`: four pull-up inputs GPIO25/26/27/32, returns active-low button mask.
- `Gas_System/components/app_tasks/button_task/`: pure C debounce plus polling FreeRTOS producer. Driver does not depend on application state.
- `control_task.c`: `dk_gui_nut` queues relative button identities; private `dich_nut` resolves current state/revision on owner. Boundaries clamp +/-10, normal core guards apply. No response credits for physical inputs.
- `Gas_System/main/Gas_System.c`: default simulation; optional real GPIO trial; temporary save-failure consumer until storage exists. No ADC/LCD/NVS/web implementation.
- `Gas_System/tests/buttons/`: deterministic debounce and fake-GPIO driver tests, 20 + 28 assertions. Runtime now 327; core 123. Demo ESP-IDF 5.5.4 build passed; hardware not flashed.
- `docs/button-task-guide.md`: source reading order, event flow, button semantics and board trial instructions. This section supersedes older README-only driver/build status below.

## Verified snapshot
- `Gas_System/CMakeLists.txt`: ESP-IDF project named Gas_System.
- `Gas_System/main/CMakeLists.txt`: registers Gas_System.c as the main component source.
- `Gas_System/main/Gas_System.c`: starts control_task in simulation mode and queues two demo commands; callback logs only, no GPIO. main/Kconfig.projbuild enables CONFIG_DK_THU_NGHIEM by default.
- `Gas_System/sdkconfig`: CONFIG_IDF_TARGET="esp32"; does not establish the physical board model.
- `Gas_System/build/`: directory exists; generated contents were not inspected.

## Current interactions
- Implemented C identifiers now use unaccented Vietnamese (`dk_` prefix); comments and runner messages use Vietnamese. Existing paths remain stable. Host verification after translation: 10 groups / 123 checks passed.
- `Gas_System/components/control_core/include/control_core.h`: implemented state/config/event/result types; dk_khoi_tao, dk_xu_ly_su_kien, dk_han_ke_tiep. One caller owns mutable state; no hardware/RTOS dependencies.
- `Gas_System/components/control_core/control_core.c`: implemented mode/readiness/hysteresis/edit/save/deadline logic. Returns side effects for future runtime; does not perform I/O.
- `Gas_System/components/control_core/CMakeLists.txt`: registers pure-C component for ESP-IDF; not yet built on ESP-IDF.
- `Gas_System/tests/control_core/`: GCC host harness, run.ps1, local out ignore. Run from workspace with `powershell -NoProfile -ExecutionPolicy Bypass -File Gas_System/tests/control_core/run.ps1`. Passed 10 suites / 123 checks; logs in out/.
- `Gas_System/components/app_tasks/control_task/`: implemented FreeRTOS service/API with private state, bounded queues, result reservation, deadline waits, output callback and snapshots; app_tasks/CMakeLists.txt registers component. Other five task folders remain README-only.
- `Gas_System/components/drivers/`: README-only module folders mq2, lcd1602, buttons and relay; no driver implementation or component CMake file.
- ESP-IDF app_main now contains a simulation startup; actual firmware build/flash not yet verified. Test fixtures are not calibrated MQ-2 configuration.
- Sensor, LCD, buttons, relay, network, web, storage and tests are not implemented in the inspected source files.

## Planning documents
- `docs/control-task-guide.md`: runtime flow, queue/credit invariants, integration API, simulator boundary, demo, known toolchain setup failures and run commands.
- `docs/code-walkthrough.md`: explanation of the implemented C core and test runner, projected full task/driver/web flow, and explicit boundaries of unfinished integration.
- G1 draft documents: `spec.md` (requirements/open P1–P4), `state-model.md` (ownership/transitions/task responsibilities), `api-contracts.md` (driver/service boundaries), `protocol.md` (web commands/results/snapshots), `test-matrix.md` (19 scenarios, all NOT RUN). These are design documents, not implemented modules.
- `docs/wiring.md`: complete proposed connections, ASCII circuit diagrams, divider calculations, BOM and G0 measurement/bring-up procedure. No hardware validation or firmware added.
- `docs/hardware.md` now includes a proposed GPIO map (34 ADC, 21/22 I2C, 25/26/27/32 buttons, 23 relay) and noise/voltage notes with primary references. No pin configuration source exists yet.
- Additional LCD photo recorded in hardware.md: backpack/contrast potentiometer visible; exact IC marking and I2C address unverified. No source changes.
- Latest hardware correction: LCD1602 uses PCF8574 backpack; previous Waveshare PDF is not applicable. MQ-2 image shows FC-22. Relay scope limited by user to active-high GPIO; electrical module investigation is no longer a software gate.
- `docs/hardware.md`: confirmed I2C LCD, MQ-2 AO/DO, discrete LED/resistor and pending electrical checks; sdkconfig reports ESP-IDF 5.5.4, while idf.py is unavailable in the current shell.
- `docs/context.md`: user-confirmed requirements, current planning authorization and unresolved choices.
- `docs/project-plan.md`: phased roadmap, component boundaries/folder layout, requirement-to-test mapping and exit criteria. Task/driver folder skeletons exist; other proposed components do not yet exist.
- Source entry point and CMake files were rechecked during planning on 2026-09-24; app_main remains empty. No implementation or architecture migration occurred.

## Validation and unknowns
- `Gas_System/tests/control_task/`: compiles real runtime source with fake FreeRTOS transport/clock. 243 checks passed (backpressure, response retention, mailbox isolation, stale deadline, storage channel and GPIO failure); not a test of RTOS scheduling. Core regressions: 123 checks pass.
- `Gas_System/tests/build_firmware.ps1`: environment-aware firmware build helper, unverified after stderr handling adjustment. ESP-IDF 5.5.4 exists locally; activation failed before compiler in this turn. No board or flash operation performed.
- GCC C11 build and deterministic host tests passed (10 suites, 123 checks) during implementation; ESP-IDF build and physical tests have not been run.
- User confirmed ESP32-DevKit V1, ESP-IDF, one 5 V / 10 A relay module and same-Wi-Fi web access on 2026-09-24; no implementation was added.
- User confirmed the switched load is a discrete LED/resistor powered by 3.3 V and the relay is active-high (HIGH on, LOW off). Electrical compatibility remains unmeasured.
- Installed ESP-IDF version, wiring, pin allocation and relay interface remain unverified.
- This document describes existing files, not a proposed architecture or implementation plan.
