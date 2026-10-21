#include <Arduino.h>
#include <Wire.h>
#include <FastLED.h>

// ---------------- Pins (ESP32-C3 Super Mini) ----------------
#define PIN_SDA       6     // MPU-6050 SDA
#define PIN_SCL       7     // MPU-6050 SCL
#define PIN_LED_DATA  10    // WS2812B DIN (through a ~330 ohm resistor)

// ---------------- Strip ----------------
#define NUM_LEDS      80
#define BRIGHTNESS    120   // 0-255, global brightness cap
#define USB_MAX_MA    450   // FastLED will dim the strip to stay under this

// ---------------- Sensitivity (tune these!) ----------------
// Vibration is measured in g, after removing gravity.
// Below NOISE_FLOOR_G nothing lights; at MAX_SHAKE_G the whole strip lights.
// The mapping is logarithmic so slight jiggles and big kicks both register.
const float NOISE_FLOOR_G = 0.005f;
const float MAX_SHAKE_G   = 0.600f;

// How fast the bar falls back down after a shake (fraction kept per frame).
const float DECAY = 0.96f;

// Peak marker: holds at the highest LED for a moment, then falls.
const uint32_t PEAK_HOLD_MS = 800;
const float    PEAK_FALL_LEDS_PER_FRAME = 0.3f;

// ---------------- MPU-6050 ----------------
const uint8_t MPU_ADDR = 0x68;   // 0x69 if AD0 is tied high

CRGB leds[NUM_LEDS];

static void mpuWrite(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

static bool mpuInit() {
  Wire.beginTransmission(MPU_ADDR);
  if (Wire.endTransmission() != 0) return false;
  mpuWrite(0x6B, 0x01);   // PWR_MGMT_1: wake up, PLL with X gyro clock
  mpuWrite(0x19, 0x04);   // SMPLRT_DIV: 1kHz / (1+4) = 200 Hz
  mpuWrite(0x1A, 0x03);   // CONFIG: DLPF ~44 Hz, filters out electrical noise
  mpuWrite(0x1C, 0x00);   // ACCEL_CONFIG: +/-2g (most sensitive)
  return true;
}

// Returns acceleration in g on each axis. False on I2C error.
static bool mpuReadAccel(float &ax, float &ay, float &az) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);       // ACCEL_XOUT_H
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(MPU_ADDR, (uint8_t)6) != 6) return false;
  int16_t x = (Wire.read() << 8) | Wire.read();
  int16_t y = (Wire.read() << 8) | Wire.read();
  int16_t z = (Wire.read() << 8) | Wire.read();
  ax = x / 16384.0f;
  ay = y / 16384.0f;
  az = z / 16384.0f;
  return true;
}

// Red (LED 1) fading to yellow (LED 80).
static CRGB gradientColor(int i) {
  uint8_t g = (uint8_t)((uint32_t)i * 255 / (NUM_LEDS - 1));
  return CRGB(255, g, 0);
}

void setup() {
  Serial.begin(115200);

  FastLED.addLeds<WS2812B, PIN_LED_DATA, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, USB_MAX_MA);
  FastLED.clear(true);

  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(400000);

  while (!mpuInit()) {
    Serial.println("MPU-6050 not found at 0x68 - check wiring");
    leds[0] = CRGB::Blue;          // blue first LED = sensor problem
    FastLED.show();
    delay(1000);
  }
  Serial.println("MPU-6050 ready");

  // Quick startup sweep so you know the strip works.
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = gradientColor(i);
    FastLED.show();
    delay(8);
  }
  delay(300);
  FastLED.clear(true);
}

void loop() {
  static float gravity = 1.0f;      // slow-moving estimate of resting magnitude
  static float level = 0.0f;        // current bar height in LEDs (0..NUM_LEDS)
  static float peak = 0.0f;
  static uint32_t peakTime = 0;
  static uint32_t lastPrint = 0;

  float ax, ay, az;
  if (!mpuReadAccel(ax, ay, az)) {
    delay(5);
    return;
  }

  // Total acceleration minus gravity = vibration.
  float mag = sqrtf(ax * ax + ay * ay + az * az);
  gravity += (mag - gravity) * 0.01f;
  float shake = fabsf(mag - gravity);

  // Log-scale map: NOISE_FLOOR_G -> 0 LEDs, MAX_SHAKE_G -> NUM_LEDS.
  float target = 0.0f;
  if (shake > NOISE_FLOOR_G) {
    target = logf(shake / NOISE_FLOOR_G) / logf(MAX_SHAKE_G / NOISE_FLOOR_G);
    target = constrain(target, 0.0f, 1.0f) * NUM_LEDS;
  }

  // Jump up instantly, fall back slowly.
  if (target > level) level = target;
  else level *= DECAY;

  uint32_t now = millis();
  if (level >= peak) {
    peak = level;
    peakTime = now;
  } else if (now - peakTime > PEAK_HOLD_MS) {
    peak = max(level, peak - PEAK_FALL_LEDS_PER_FRAME);
  }

  int lit = (int)level;
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = (i < lit) ? gradientColor(i) : CRGB::Black;
  }
  int p = (int)peak - 1;
  if (p >= 0 && p < NUM_LEDS) leds[p] = gradientColor(p);
  FastLED.show();

  // Serial output for tuning NOISE_FLOOR_G / MAX_SHAKE_G.
  if (now - lastPrint > 100) {
    lastPrint = now;
    Serial.printf("shake=%.3fg  leds=%d\n", shake, lit);
  }

  delay(5);   // ~200 Hz, matches the sensor sample rate
}
