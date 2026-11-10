# DHT11 Digital Temperature & Humidity Sensor Reference

## Electrical & Operating Specifications

| Parameter | Rating |
| --- | --- |
| Supply Voltage (`VCC`) | `3.3 V` to `5.5 V` DC |
| Operating Current | `0.3 mA` measuring, `60 uA` standby |
| Temperature Range | `0 C` to `50 C` (`+/- 2 C` accuracy) |
| Relative Humidity Range | `20 %` to `90 %` RH (`+/- 5 %` RH accuracy) |
| Minimum Sampling Interval | `>= 1.0 s` (`2.0 s` recommended) |
| Pull-Up Resistor | `4.7 kOhm` to `10 kOhm` between `DATA` and `VCC` |

## Single-Wire 40-Bit Frame Protocol

1. **Host Start Signal**:
   - Pull `DATA` `LOW` for at least `18 ms` (`20 ms` typical), then pull `HIGH` for `20-40 us` and switch the GPIO to `INPUT_PULLUP`.
2. **Sensor Presence Handshake**:
   - Sensor pulls `DATA` `LOW` for `~80 us`, followed by `HIGH` for `~80 us`.
3. **40 Data Bits (MSB First)**:
   - Every bit starts with a `~50 us` `LOW` pre-amble.
   - `HIGH` pulse duration encodes the bit value:
     - `26-28 us` (`HIGH < LOW`) -> `0`
     - `~70 us` (`HIGH > LOW`) -> `1`
4. **5-Byte Payload Layout**:
   - `Byte 0`: Integral Relative Humidity (`% RH`)
   - `Byte 1`: Decimal Relative Humidity (`0` on standard DHT11, `0-9` on DHT11-Plus)
   - `Byte 2`: Integral Temperature (`C`)
   - `Byte 3`: Decimal Temperature (bit 7 indicates negative sign on extended variants)
   - `Byte 4`: Checksum = `(Byte 0 + Byte 1 + Byte 2 + Byte 3) & 0xFF`
