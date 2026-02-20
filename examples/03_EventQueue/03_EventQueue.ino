#include <Arduino.h>
#include <JWMatrixButtons.h>

static const uint8_t ROW_PINS[2] = { 25, 26 };
static const uint8_t COL_PINS[4] = { 35, 34, 39, 36 };

enum BtnId : uint8_t { BTN_INFO = 0,
                       BTN_CONFIG,
                       BTN_LEFT,
                       BTN_START,
                       BTN_RIGHT,
                       BTN_UP,
                       BTN_DOWN,
                       BTN__COUNT };

static const JWMatrixButtons::BtnMapItem BTN_MAP[] = {
  { BTN_INFO, 0, 0 }, { BTN_CONFIG, 0, 1 }, { BTN_LEFT, 0, 2 }, { BTN_START, 0, 3 }, { BTN_RIGHT, 1, 0 }, { BTN_UP, 1, 1 }, { BTN_DOWN, 1, 2 }
};

JWMatrixButtons btn;

static const char* evName(JWMatrixButtons::EvType t) {
  switch (t) {
    case JWMatrixButtons::EV_PRESS: return "PRESS";
    case JWMatrixButtons::EV_RELEASE: return "RELEASE";
    case JWMatrixButtons::EV_REPEAT: return "REPEAT";
    default: return "?";
  }
}

void setup() {
  Serial.begin(115200);
  btn.begin(ROW_PINS, 2, COL_PINS, 4, BTN_MAP, sizeof(BTN_MAP) / sizeof(BTN_MAP[0]), BTN__COUNT, false, 35);

  btn.setRepeatEnabled(BTN_UP, true);
  btn.setRepeatEnabled(BTN_DOWN, true);

  Serial.println("Event queue demo listo.");
}

void loop() {
  btn.update();

  for (uint8_t i = 0; i < btn.eventCount(); i++) {
    JWMatrixButtons::BtnEvent e;
    if (btn.getEvent(i, e)) {
      Serial.print("id=");
      Serial.print(e.id);
      Serial.print(" ");
      Serial.print(evName(e.type));
      Serial.print(" mult=");
      Serial.print(e.mult);
      Serial.print(" held=");
      Serial.println(e.held_ms);
    }
  }

  delay(5);
}
