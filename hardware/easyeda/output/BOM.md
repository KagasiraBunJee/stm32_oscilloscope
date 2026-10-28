# A0 draft BOM

Not a purchase/release list. No supplier availability verified.

| Qty | References | Value / part | Package | Requirement |
|---:|---|---|---|---|
| 1 | J1 | CH1 DIRECT 0-3.3V | HDR1X2 |  |
| 1 | J2 | CH1 HIGH +/-15V | HDR1X2 |  |
| 3 | R1, R13, R25 | 100R | R0805 | 1%, >=0.125 W, >=50 V |
| 6 | C1, C2, C3, C4, C5, C6 | 1nF | C0805 | C0G, 5%, >=25 V |
| 6 | D1, D3, D4, D6, D7, D9 | BAT54S | SOT23 | Nexperia BAT54S, series pair; pin 3 midpoint |
| 31 | R2, R3, R4, R5, R6, R7, R8, R9, R10, R11, R14, R15, R16, R17, R18, R19, R20, R21, R22, R23, R26, R27, R28, R29, R30, R31, R32, R33, R34, R35, R39 | 100k | R0805 | 0.1%, <=25 ppm/K, >=0.125 W, >=50 V |
| 3 | D2, D5, D8 | BAS70-04 | SOT23 | Series-pair SOT23; verify chosen manufacturer pin 3 midpoint |
| 3 | R12, R24, R36 | 33R | R0805 | 1%, >=0.125 W, >=50 V |
| 1 | J3 | CH2 DIRECT 0-3.3V | HDR1X2 |  |
| 1 | J4 | CH2 HIGH +/-15V | HDR1X2 |  |
| 1 | J5 | CH3 DIRECT 0-3.3V | HDR1X2 |  |
| 1 | J6 | CH3 HIGH +/-15V | HDR1X2 |  |
| 1 | J7 | POWER 3.3V | HDR1X2 |  |
| 1 | J8 | STM32 OUTPUTS | HDR2X4 |  |
| 1 | FB1 | Ferrite 600R@100MHz | R0805 | >=100 mA, DCR <=0.5 ohm; verify rail drop during test |
| 1 | C7 | 10uF | C0805 | X7R, 10%, >=16 V |
| 2 | C8, C9 | 100nF | C0805 | X7R, 10%, >=16 V |
| 2 | R37, R38 | 40k | R0805 | 0.1%, <=25 ppm/K, >=0.125 W, >=50 V |
| 1 | U1 | TLV9064IDR | SOIC14 | TI TLV9064IDR, SOIC-14 D package; 3.3 V quad buffer |

J1/J3/J5: direct CH1/2/3. J2/J4/J6: conditioned CH1/2/3. Pin 1 signal, pin 2 GND.

J7: pin 1 +3.3 V, pin 2 GND. J8: 1 PA1, 2 PC0, 3 PA2, 4 PC1, 5 PA3, 6 PC2, 7/8 GND.

Headers: 2.54 mm pitch, 1 mm finished holes; select physical part before ordering.

Off-board: six signal/ground cables or panel connectors; four M3 screws, insulated spacers, nuts. Length depends on the revised enclosure.

Capacitor effective capacitance, diode pinouts, all land patterns, rail injection, power-off behavior, bandwidth and DRC remain release checks.
