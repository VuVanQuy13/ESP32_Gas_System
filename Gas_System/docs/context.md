# Project Context

## Purpose
ESP-IDF firmware for an ESP32 gas monitoring and relay control system using an MQ-2 sensor, physical buttons, LCD, NVS threshold storage, a web interface, and optional Telegram notifications.

## Architecture
- `main/Gas_System.c` initializes shared hardware and starts the application services and FreeRTOS tasks.
- `components/control_core` contains the hardware-independent control policy and state transitions.
- `components/app_tasks` owns the control, sensor, button, storage, and display tasks.
- `components/drivers` contains GPIO, ADC, relay, button, and LCD drivers.
- `components/storage`, `components/network`, `components/web_service`, and `components/telegram_notify` provide persistence and external interfaces.

## Current Task Decision
- The user's phrase "explanatory command" is interpreted as explanatory source comments.
- Remove comments from source files while preserving executable statements, preprocessor directives, and string literals.
- Scope: C, header, and JavaScript source files under `main/` and `components/`.

## Validation
- Review the diff for comment-only removals and check remaining comment markers against string false positives.
- Do not run tests unless requested.
