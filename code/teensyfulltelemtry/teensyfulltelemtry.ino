#include <SPI.h>
#include <mcp2515.h>

#define FARDRIVER_SERIAL Serial1
#define FARDRIVER_BAUD 19200

#define ESP32_SERIAL Serial2
#define ESP32_BAUD 115200

#define CAN_CS_PIN 10
MCP2515 mcp2515(CAN_CS_PIN);

#define CAN_ID_MOTOR   0x100
#define CAN_ID_BATTERY 0x101

const uint8_t flash_read_addr[55] = {
  0xE2, 0xE8, 0xEE, 0x00, 0x06, 0x0C, 0x12,
  0xE2, 0xE8, 0xEE, 0x18, 0x1E, 0x24, 0x2A,
  0xE2, 0xE8, 0xEE, 0x30, 0x5D, 0x63, 0x69,
  0xE2, 0xE8, 0xEE, 0x7C, 0x82, 0x88, 0x8E,
  0xE2, 0xE8, 0xEE, 0x94, 0x9A, 0xA0, 0xA6,
  0xE2, 0xE8, 0xEE, 0xAC, 0xB2, 0xB8, 0xBE,
  0xE2, 0xE8, 0xEE, 0xC4, 0xCA, 0xD0,
  0xE2, 0xE8, 0xEE, 0xD6, 0xDC, 0xF4, 0xFA
};

uint8_t fdFrame[16];
uint8_t fdFrameLen = 0;

struct Telemetry {
  float rpm = 0;
  float voltage = 0;
  float line_current = 0;
  float speed_kmh = 0;
  float soc_percent = 0;
  uint32_t fd_update_ms = 0;

  uint16_t measure_speed_raw = 0;
  int16_t deci_volts = 0;
  int16_t zero_batt_coeff = 0;
  int16_t full_batt_coeff = 0;
  uint8_t wheel_radius = 0;
  uint8_t wheel_width = 0;
  uint8_t wheel_ratio = 0;
  uint16_t rate_ratio = 1;
} telemetry;

void setup() {
  Serial.begin(115200);
  FARDRIVER_SERIAL.begin(FARDRIVER_BAUD);
  ESP32_SERIAL.begin(ESP32_BAUD);

  SPI.begin();
  mcp2515.reset();
  mcp2515.setBitrate(CAN_500KBPS, MCP_8MHZ);
  mcp2515.setNormalMode();
}

void loop() {
  readFardriver();
  computeDerived();

  static uint32_t lastSend = 0;
  if (millis() - lastSend >= 100) {
    lastSend = millis();
    sendJSON();
    sendCAN();
  }
}

void readFardriver() {
  while (FARDRIVER_SERIAL.available()) {
    uint8_t b = FARDRIVER_SERIAL.read();
    if (fdFrameLen == 0 && b != 0xAA) continue;
    fdFrame[fdFrameLen++] = b;
    if (fdFrameLen == 16) {
      handleFardriverFrame(fdFrame);
      fdFrameLen = 0;
    }
  }
}

void handleFardriverFrame(uint8_t *f) {
  uint8_t id = f[1] & 0x3F;
  if (id >= 55) return;
  uint8_t addr = flash_read_addr[id];
  uint16_t w0 = f[2] | (f[3] << 8);
  uint16_t w1 = f[4] | (f[5] << 8);

  if (addr == 0xE2) {
    telemetry.measure_speed_raw = w0;
    telemetry.rpm = w0 / 3.92f;
    telemetry.fd_update_ms = millis();
  } else if (addr == 0xE8) {
    telemetry.deci_volts = (int16_t)w0;
    telemetry.voltage = w0 / 10.0f;
    telemetry.line_current = (int16_t)w1 / 4.0f;
    telemetry.fd_update_ms = millis();
  } else if (addr == 0x0C) {
    telemetry.zero_batt_coeff = (int16_t)(f[4] | (f[5] << 8));
    telemetry.full_batt_coeff = (int16_t)(f[6] | (f[7] << 8));
  } else if (addr == 0xD0) {
    telemetry.wheel_ratio = f[6];
    telemetry.wheel_radius = f[7];
    telemetry.wheel_width = f[9];
    telemetry.rate_ratio = f[10] | (f[11] << 8);
  }
}

void computeDerived() {
  if (telemetry.rate_ratio > 0) {
    float factor = 0.00376991136f *
      (telemetry.wheel_radius * 1270.f + telemetry.wheel_width * telemetry.wheel_ratio) /
      telemetry.rate_ratio;
    telemetry.speed_kmh = telemetry.measure_speed_raw * factor;
  }

  int16_t span = telemetry.full_batt_coeff - telemetry.zero_batt_coeff;
  if (span != 0) {
    float pct = 100.0f * (telemetry.deci_volts - telemetry.zero_batt_coeff) / span;
    telemetry.soc_percent = constrain(pct, 0.0f, 100.0f);
  }
}

void sendJSON() {
  ESP32_SERIAL.print("{");
  ESP32_SERIAL.print("\"rpm\":");           ESP32_SERIAL.print(telemetry.rpm, 1);
  ESP32_SERIAL.print(",\"voltage\":");      ESP32_SERIAL.print(telemetry.voltage, 1);
  ESP32_SERIAL.print(",\"line_current\":"); ESP32_SERIAL.print(telemetry.line_current, 2);
  ESP32_SERIAL.print(",\"speed_kmh\":");    ESP32_SERIAL.print(telemetry.speed_kmh, 1);
  ESP32_SERIAL.print(",\"soc\":");          ESP32_SERIAL.print(telemetry.soc_percent, 0);
  ESP32_SERIAL.println("}");
}

void sendCAN() {
  struct can_frame frame;

  uint16_t rpm = (uint16_t)telemetry.rpm;
  uint16_t speed10 = (uint16_t)(telemetry.speed_kmh * 10);

  frame.can_id = CAN_ID_MOTOR;
  frame.can_dlc = 4;
  frame.data[0] = lowByte(rpm);
  frame.data[1] = highByte(rpm);
  frame.data[2] = lowByte(speed10);
  frame.data[3] = highByte(speed10);
  mcp2515.sendMessage(&frame);

  uint16_t volts10 = (uint16_t)(telemetry.voltage * 10);
  int16_t amps10 = (int16_t)(telemetry.line_current * 10);
  uint8_t soc = (uint8_t)telemetry.soc_percent;

  frame.can_id = CAN_ID_BATTERY;
  frame.can_dlc = 5;
  frame.data[0] = lowByte(volts10);
  frame.data[1] = highByte(volts10);
  frame.data[2] = lowByte(amps10);
  frame.data[3] = highByte(amps10);
  frame.data[4] = soc;
  mcp2515.sendMessage(&frame);
}