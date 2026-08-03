# HTTP API: IAQ Touch VSP Minimum RPM

## Endpoint

```http
GET /api/Pump_<number>/VSPMinimum/set?value=<rpm>
```

## Parameters

| Parameter | Allowed values | Description |
|---|---|---|
| `number` | `1` through `4` | IAQ Touch pump position |
| `value` | `600` through `3450` | Requested minimum speed in RPM |

## Examples

```bash
curl -i   'http://AQUALINKD_HOST/api/Pump_1/VSPMinimum/set?value=2000'
```

```bash
curl -i   'http://AQUALINKD_HOST/api/Pump_2/VSPMinimum/set?value=600'
```

## Accepted response

```http
HTTP/1.1 200 OK
Content-Type: text/plain

Ok
```

`200 OK` confirms request acceptance. Programming is asynchronous. Confirm the
final result in the AquaLinkD log.

Successful example:

```text
VSP minimum write PASS: Pump 1 requested=2000 verified=2000 display='2000 RPM '
```

## Home Assistant example

```yaml
rest_command:
  aqualinkd_set_pump_1_minimum:
    url: >-
      http://AQUALINKD_HOST/api/Pump_1/VSPMinimum/set?value={{ rpm }}
    method: GET
```

Example call:

```yaml
action: rest_command.aqualinkd_set_pump_1_minimum
data:
  rpm: 2000
```

## Operational behavior

AquaLinkD prefers live field metadata and falls back to startup-cached keycodes
when later VSP Setup pages omit field definitions. Every write is verified by
controller read-back.

## Known limitations

- Minimum RPM is the only writable VSP setting.
- Pump 3 and Pump 4 were not installed in the validation environment.
