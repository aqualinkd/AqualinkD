# Pull Request Draft

## Title

Add verified IAQ Touch VSP minimum-speed programming API

## Summary

This change adds an HTTP endpoint for programming the minimum RPM of IAQ Touch
variable-speed pumps.

It supports Pump 1 through Pump 4, validates RPM values, caches minimum-field
keycodes learned during startup, and performs controller read-back verification.

## Motivation

Some IAQ Touch controllers provide the complete VSP Setup field table during
startup discovery but omit those field definitions on later visits. The
controller still accepts the previously discovered keycode and RPM command.

## Implementation

- parameterized IAQ Touch VSP minimum programmer
- `/api/Pump_<number>/VSPMinimum/set?value=<rpm>`
- Pump 1 through Pump 4 validation
- 600 through 3450 RPM validation
- startup keycode cache
- live-field preference with cached fallback
- controller read-back verification

## Validation

| Test | Result |
|---|---|
| Startup cache | Four minimum keycodes cached |
| Pump 1 at 2000 RPM | PASS |
| Pump 2 at 600 RPM | PASS |
| Later incomplete page | Cached fallback used |
| Service health | HTTP 200 |

## Known limitations

- Minimum RPM only.
- Pump 3 and Pump 4 were not installed.
- Cache is in memory and rebuilt at startup.
- Broader controller and firmware testing is desirable.
