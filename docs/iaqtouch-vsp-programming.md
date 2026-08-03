cd /opt/nicebear/src/AqualinkD

cat > docs/iaqtouch-vsp-programming.md <<'EOF'
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
