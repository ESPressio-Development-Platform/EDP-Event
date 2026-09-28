# TemperatureMonitor

Complete buildable application corresponding to the step-by-step `README.MD` walkthrough. It defines one bounded `TemperatureChanged` Event, one Dedicated Thread Listener, exact Memory occurrence storage, a typed callback provider, runtime subscription, Dispatch, bounded Drain and cooperative shutdown.

Build with PlatformIO:

```text
pio run
```

The application prints `TemperatureMonitor: PASS` after receiving the emitted 23.45 C value.
