# ShakeMeter

A vibration "VU meter" for your desk. An accelerometer measures how much the desk is shaking and lights a WS2812B LED strip as a bar graph. A slight jiggle lights the first LED; a hard, sustained leg-bounce runs the bar all the way to LED 80. Colours fade from red at LED 1 to yellow at the end of the strip.

Built to gently let a coworker know their leg-bouncing shakes the whole row of cubicles.

## Hardware

| Part | Notes |
|------|-------|
| ESP32-C3 Super Mini | Any ESP32-C3 board works; pins below are for the Super Mini |
| MPU-6050 accelerometer/gyro (GY-521 breakout) | I2C, address 0x68 |
| WS2812B LED strip, 80 LEDs | 5V addressable RGB |
| 330 Ω resistor | In series with the strip's data line |
| 470–1000 µF electrolytic capacitor (optional) | Across the strip's 5V and GND |
| Breadboard, jumper wires, USB-C cable | Powered from a USB port |

## Wiring

```
ESP32-C3 Super Mini          MPU-6050 (GY-521)
-------------------          -----------------
3V3  ----------------------- VCC
GND  ----------------------- GND
GPIO6 ---------------------- SDA
GPIO7 ---------------------- SCL
                             (AD0, INT, XDA, XCL unconnected)

ESP32-C3 Super Mini          WS2812B strip (input end)
-------------------          -------------------------
5V   ----------------------- +5V
GND  ----------------------- GND
GPIO10 ---[ 330 Ω ]--------- DIN
```

GPIO 2, 8 and 9 are avoided because they are boot-strapping pins on the ESP32-C3 (8 is also the onboard LED and 9 is the BOOT button).

The capacitor goes across the strip's +5V and GND; mind the polarity (the striped leg goes to GND).

## Power

The whole thing runs from the board's USB port. A full 80-LED strip at full white could draw about 4.8 A, far more than USB can supply, so the firmware limits it:

- `USB_MAX_MA` (450 mA): FastLED dims the strip to stay under this.
- `BRIGHTNESS` (120 of 255): global brightness cap.

If the board resets or flickers when the bar is full, lower `USB_MAX_MA`.

## Building and flashing

1. Install [VS Code](https://code.visualstudio.com/) with the [PlatformIO](https://platformio.org/) extension.
2. Open this folder in VS Code.
3. Connect the ESP32-C3 over USB and click PlatformIO **Upload**.

FastLED is fetched automatically (pinned in [platformio.ini](platformio.ini)). Serial output goes over the C3's native USB at 115200 baud.

## What it does

- **Startup:** a red-to-yellow sweep runs along the strip. If the first LED stays **blue**, the MPU-6050 wasn't found; check SDA/SCL.
- **Sensing:** reads the accelerometer at about 200 Hz, subtracts gravity, and maps the remaining vibration to a bar height on a logarithmic scale.
- **Display:** the bar jumps up instantly and decays slowly. A peak dot holds at the highest point for 0.8 s, like a stereo VU meter.

## Tuning

Sensitivity depends on where the sensor is mounted. Open the PlatformIO Serial Monitor and watch the `shake=0.xxxg` readings, then adjust the constants at the top of [src/main.cpp](src/main.cpp):

| Constant | Default | Change it when |
|----------|---------|----------------|
| `NOISE_FLOOR_G` | 0.015 | LEDs light with nobody touching the desk: raise it |
| `MAX_SHAKE_G` | 0.60 | The bar never reaches the end: lower it |
| `DECAY` | 0.96 | Closer to 1.0 makes the bar linger longer |

Fix the MPU-6050 firmly to the desk or cubicle wall (tape or hot glue). A loose breadboard rattles and gives noisy readings.
