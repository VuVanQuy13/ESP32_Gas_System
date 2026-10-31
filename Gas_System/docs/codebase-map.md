# Codebase Map

## Snapshot
- Purpose: ESP-IDF firmware for gas monitoring and relay control.
- Stack: C, ESP-IDF, FreeRTOS; browser interface in HTML/CSS/JavaScript.
- Entry point: `main/Gas_System.c` (`app_main`).

## Top-Level Layout
- `main/`: startup and component registration.
- `components/app_tasks/`: FreeRTOS task implementations.
- `components/control_core/`: control policy and state transitions.
- `components/drivers/`: board hardware drivers and pin configuration.
- `components/storage/`: NVS persistence.
- `components/network/`: Wi-Fi startup.
- `components/web_service/`: HTTP/WebSocket gateway and embedded web UI.
- `components/telegram_notify/`: optional Telegram notifications.

## Module Map
| Module | Key files | Responsibility and dependencies |
| --- | --- | --- |
| Startup | `main/Gas_System.c` | Initializes shared hardware and starts services/tasks; consumes app tasks and network/web components. |
| Control | `components/control_core/control_core.c`, `components/app_tasks/control_task/` | Applies events and serializes system state; tasks use drivers, storage, and notification services. |
| I/O tasks | `components/app_tasks/{sensor,button,display}_task/` | Reads sensor/buttons and publishes display state through the control service. |
| Persistence | `components/storage/`, `components/app_tasks/storage_task/` | Reads and writes thresholds to NVS. |
| Interfaces | `components/web_service/`, `components/telegram_notify/` | Serves web UI and optional notifications. |
| Hardware | `components/drivers/` | GPIO, MQ-2 ADC, relay, buttons, and LCD drivers. |

## Interaction Map
- `app_main` initializes drivers/storage and starts control, storage, sensor, display, and button tasks.
- Tasks submit events to the control task; web requests use the gateway and the same control service.
- The control task emits state snapshots consumed by display and web paths; storage task performs persistence.
- Network startup precedes web server startup. Telegram worker is optional through configuration.

## Validation Guide
- Build system: ESP-IDF CMake component registration in the root and component `CMakeLists.txt` files.
- A project-specific test command was not identified during this scope review.

## Unknowns
- Board wiring and sensor calibration assumptions depend on the configured GPIOs and physical hardware.
