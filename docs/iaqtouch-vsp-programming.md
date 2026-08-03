# IAQ Touch VSP Minimum-Speed Programming

## Overview

AquaLinkD supports programming the minimum RPM of IAQ Touch variable-speed
pumps through its HTTP API.

The implementation supports Pump 1 through Pump 4, validates each request, and
verifies the final value by reading it back from the controller.

The current implementation is limited to the VSP minimum-RPM field.

## Supported environment

The feature requires:

- IAQ Touch programming mode
- Extended protocol support
- A controller exposing the VSP Setup page
- A variable-speed pump assigned to the requested pump position

Validation was completed on a Jandy AquaLink RS-8 Combo system with:

- Pump 1 configured as Filter Pump
- Pump 2 configured as Water Feature Pump

## VSP Setup page layout

The IAQ Touch VSP Setup page exposes up to four pump columns.

| Field | Pump 1 | Pump 2 | Pump 3 | Pump 4 |
|---|---:|---:|---:|---:|
| Model | 0 | 1 | 2 | 3 |
| Application | 4 | 5 | 6 | 7 |
| Minimum | 8 | 9 | 10 | 11 |
| Maximum | 12 | 13 | 14 | 15 |
| Prime speed | 16 | 17 | 18 | 19 |
| Prime duration | 20 | 21 | 22 | 23 |

The minimum-speed field index is:

```text
8 + pump number - 1
```

Observed minimum-field keycodes were:

| Pump | Field index | Keycode |
|---|---:|---:|
| Pump 1 | 8 | `0x19` |
| Pump 2 | 9 | `0x1a` |
| Pump 3 | 10 | `0x1b` |
| Pump 4 | 11 | `0x1c` |

## Controller behavior

During startup discovery, the controller can return a complete VSP Setup page
containing labels and keycodes for its programmable fields.

Some later visits return the page lifecycle without retransmitting those field
definitions. In that condition, the transient IAQ Touch button table is empty,
even though the controller still accepts the previously discovered field
keycode and RPM programming command.

## Cached-keycode design

AquaLinkD records each valid minimum-field keycode during startup VSP
discovery. The cache is held in memory and rebuilt whenever AquaLinkD starts.

For each write, the programmer:

1. Validates the pump number.
2. Validates the requested RPM.
3. Navigates to VSP Setup.
4. Uses the live field and refreshes the cache when metadata is available.
5. Uses the cached keycode when live field metadata is unavailable.
6. Sends the requested RPM.
7. Reads the returned minimum-speed value.
8. Reports `PASS` only when the returned RPM matches the request.

## Request validation

The implementation rejects:

- pump numbers outside 1 through 4
- RPM values below 600
- RPM values above 3450
- requests for which neither a live nor cached keycode is available

## Read-back verification

An HTTP response confirms that AquaLinkD accepted the request and launched the
programming operation. Hardware programming completes asynchronously.

A write is successful only when the controller returns the requested RPM.

Example:

```text
VSP minimum writer using cached field: Pump 2 index=9 keycode=0x1a
VSP minimum write begin: Pump 2 index=9 current='<cached>' requested=600 keycode=0x1a
VSP minimum write PASS: Pump 2 requested=600 verified=600 display='600 RPM '
```

## Validation completed

- startup discovery of all four minimum-field keycodes
- cached-keycode fallback after an incomplete later VSP page
- Pump 1 minimum-speed programming at 2000 RPM
- Pump 2 minimum-speed programming at 600 RPM
- controller read-back verification

## Known limitations

- Only minimum RPM is currently writable.
- Pump 3 and Pump 4 were not installed in the validation system.
- Validation has been completed on one controller and pump topology.
- The keycode cache is process-local and rebuilt at startup.
- HTTP `200 OK` indicates request acceptance, not verified hardware success.

## Future development

The same architecture can be extended to support maximum RPM, prime speed,
prime duration, pump application, and richer Home Assistant entities.
