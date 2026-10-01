#include <Arduino.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>

// 聲音吊飾公版程式（ATtiny85）
// BZ1 -> PB1，實體第 6 腳
// SW1 -> PB2 / INT0，實體第 7 腳；按下時接到 GND
const uint8_t BUZZER_PIN = 1;
const uint8_t BUTTON_PIN = 2;

// ===== 只需要修改這一區 =====
// 音符頻率，單位：Hz；0 代表休止，不發出聲音。
const uint16_t melody[] PROGMEM = {
  523, 659, 784, 0
};

// 每一步的完整時間，單位：毫秒（ms），包含發聲與斷音間隔。
// 數量、順序必須和 melody[] 完全相同。
const uint16_t noteStepMs[] PROGMEM = {
  150, 150, 300, 50
};
// ===== 修改區結束 =====

static_assert(
  sizeof(melody) / sizeof(melody[0]) ==
    sizeof(noteStepMs) / sizeof(noteStepMs[0]),
  "melody and noteStepMs must contain the same number of items."
);

const uint8_t NOTE_COUNT = sizeof(melody) / sizeof(melody[0]);

ISR(INT0_vect)
{
  GIMSK &= ~_BV(INT0);
}

void playSoundEffect()
{
  for (uint8_t i = 0; i < NOTE_COUNT; i++) {
    const uint16_t frequency = pgm_read_word(&melody[i]);
    const uint16_t stepMs = pgm_read_word(&noteStepMs[i]);

    if (frequency > 0) {
      // 發聲時間使用該步驟的 90%，留下約 10% 的斷音間隔。
      tone(BUZZER_PIN, frequency, stepMs * 9 / 10);
    }

    delay(stepMs);
    noTone(BUZZER_PIN);
  }

  digitalWrite(BUZZER_PIN, LOW);
}

void waitForButtonRelease()
{
  uint8_t stableReleaseMs = 0;

  // 按鈕連續放開 30 ms 後才算真正放開。
  while (stableReleaseMs < 30) {
    if (digitalRead(BUTTON_PIN) == HIGH) {
      stableReleaseMs++;
    } else {
      stableReleaseMs = 0;
    }
    delay(1);
  }
}

void enterDeepSleep()
{
  noTone(BUZZER_PIN);
  digitalWrite(BUZZER_PIN, LOW);
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);

  noInterrupts();

  // 若按鈕已按下，就直接播放，不進入睡眠。
  if ((PINB & _BV(PB2)) == 0) {
    interrupts();
    return;
  }

  MCUCR &= ~(_BV(ISC01) | _BV(ISC00));
  GIFR = _BV(INTF0);
  GIMSK |= _BV(INT0);

  sleep_enable();

  #if defined(BODS) && defined(BODSE)
    sleep_bod_disable();
  #endif

  interrupts();
  sleep_cpu();
  sleep_disable();
}

void setup()
{
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // 未使用的接腳開啟內建上拉，並關閉 ADC 與比較器以降低耗電。
  pinMode(0, INPUT_PULLUP);
  pinMode(3, INPUT_PULLUP);
  pinMode(4, INPUT_PULLUP);
  ADCSRA &= ~_BV(ADEN);
  ACSR |= _BV(ACD);
}

void loop()
{
  enterDeepSleep();
  playSoundEffect();
  waitForButtonRelease();
}
