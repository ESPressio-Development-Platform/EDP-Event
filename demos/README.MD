# EDP-Event demonstrations

Each logical Event feature demonstration is maintained for Arduino IDE, PIOArduino Arduino-ESP32 and PIOArduino ESP-IDF. The directory names remain `PlatformIO_*` because PIOArduino consumes the same project format.

- `local-broadcast` — complete Queue-based local Event emission, Dedicated-Thread wake/drain, typed callback and cooperative shutdown. This is the same scenario used by the README `TemperatureMonitor` walkthrough.
- `newest-only-retention` — NewestOnly supersession plus application-bound monotonic Clock and timed-expiry rejection.
- `transport-boundary` — LocalAndRemote result separation, RemoteOnly outbound handoff and inbound LocalOnly `Ingress` using a fake bounded Transport callable.

`tests/run_demo_builds.sh` builds all PIOArduino project surfaces in coherent sibling-source mode during repository development.
