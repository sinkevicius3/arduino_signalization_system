#include <EEPROM.h>

#define LDR_PIN A0
#define SYSTEM_BTN_PIN 2
#define CALIBR_BTN_PIN 3
#define ALARM_LED_PIN 4
#define LASER_PIN 5
#define BUZZER_PIN 6
#define SYSTEM_LED_PIN 7

const int default_ldr_threshold = 200;
const int min_beam_value = 400;

const unsigned long debounce_time = 150;

const int beep_tick_interval = 10;    // 10 * 10ms = 100ms
const int print_tick_interval = 20;   // 20 * 10ms = 200ms

const byte EEPROM_MAGIC = 0xAC;
const byte EEPROM_VERSION = 1;

const int ADDR_MAGIC = 0;
const int ADDR_VERSION = 1;
const int ADDR_STATE = 2;
const int ADDR_THRESHOLD = 3; // two bytes starting at addr = 3, since threshold value is integer

int light_threshold = default_ldr_threshold;

bool beep_on = false;
int beep_ticks = 0;

int ldr_value = 0;
int ldr_print_ticks = 0;

enum State {
  OFF, ON, DETECTED
};

State system_state = OFF;

volatile bool sys_btn_flag = false;
volatile bool cal_btn_flag = false;
volatile bool timer_flag = false;

volatile unsigned long last_btn_interrupt_time = 0;
volatile unsigned long last_cal_interrupt_time = 0;

void setup() {
  Serial.begin(9600);

  pinMode(SYSTEM_BTN_PIN, INPUT_PULLUP);
  pinMode(CALIBR_BTN_PIN, INPUT_PULLUP);
  pinMode(LASER_PIN, OUTPUT);
  pinMode(ALARM_LED_PIN, OUTPUT);
  pinMode(SYSTEM_LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  bool start_on = load_settings();

  noInterrupts();
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1 = 0;                            // timer itself
  OCR1A = 156;                          // 10 ms / 0.064 ms = 156.25
  TCCR1B |= (1 << WGM12);               // CTC mode: clear timer on compare match
  TIMSK1 |= (1 << OCIE1A);              // enable compare match A interrupt
  TCCR1B |= (1 << CS12) | (1 << CS10);  // set prescaler 1024, this also starts the timer
  interrupts();

  attachInterrupt(digitalPinToInterrupt(SYSTEM_BTN_PIN), sys_btn_isr, CHANGE);
  attachInterrupt(digitalPinToInterrupt(CALIBR_BTN_PIN), cal_btn_isr, CHANGE);

  if (start_on) {
    set_state(ON);
    Serial.println("Restored: ON");
  } else {
    set_state(OFF);
    Serial.println("Restored: OFF");
  }
}

void loop() {

  if (sys_btn_flag) {
    sys_btn_flag = false;

    if (system_state == OFF) {
      set_state(ON);
      save_state(true);
      Serial.println("System ON");
    } else {
      set_state(OFF);
      save_state(false);
      Serial.println("System OFF");
    }
  }


  if (cal_btn_flag) {
    cal_btn_flag = false;

    if (system_state == ON && ldr_value >= min_beam_value) {
      light_threshold = ldr_value / 2;
      save_threshold();
      tone(BUZZER_PIN, 2000, 200);
      Serial.print("LDR calibrated with threshold = ");
      Serial.println(light_threshold);
    } else {
      tone(BUZZER_PIN, 300, 400);
      Serial.println("Calibration failed: system must be ON with the beam on the LDR");
    }
  }

  if (timer_flag) {
    timer_flag = false;
    ldr_value = analogRead(LDR_PIN);

    ++ldr_print_ticks;
    if (ldr_print_ticks >= print_tick_interval) {
      ldr_print_ticks = 0;
      Serial.println(ldr_value);
    }

    switch (system_state) {
      case OFF:
        break;
      case ON:
        if (ldr_value < light_threshold) {
          set_state(DETECTED);
          Serial.println("LASER INTERRUPTED!");
        }
        break;
      case DETECTED:
        if (ldr_value >= light_threshold) {
          set_state(ON);
          Serial.println("Laser restored!");
        } else {
          update_beep();
        }
        break;
      default:
        break;
    }
  }
}

ISR(TIMER1_COMPA_vect) {
  timer_flag = true;
}

void sys_btn_isr() {
  unsigned long now = millis();
  if (now - last_btn_interrupt_time >= debounce_time && digitalRead(SYSTEM_BTN_PIN) == LOW) {
    sys_btn_flag = true;
  }
  last_btn_interrupt_time = now;
}

void cal_btn_isr() {
  unsigned long now = millis();
  if (now - last_cal_interrupt_time >= debounce_time && digitalRead(CALIBR_BTN_PIN) == LOW) {
    cal_btn_flag = true;
  }
  last_cal_interrupt_time = now;
}

void set_state(State new_state) {
  system_state = new_state;

  switch (new_state) {
    case OFF:
      digitalWrite(LASER_PIN, LOW);
      digitalWrite(SYSTEM_LED_PIN, LOW);
      digitalWrite(ALARM_LED_PIN, LOW);
      noTone(BUZZER_PIN);
      break;

    case ON:
      digitalWrite(LASER_PIN, HIGH);
      digitalWrite(SYSTEM_LED_PIN, HIGH);
      digitalWrite(ALARM_LED_PIN, LOW);
      noTone(BUZZER_PIN);
      break;

    case DETECTED:
      digitalWrite(LASER_PIN, HIGH);
      digitalWrite(SYSTEM_LED_PIN, HIGH);
      digitalWrite(ALARM_LED_PIN, HIGH);
      beep_on = true;
      beep_ticks = 0;
      tone(BUZZER_PIN, 1000);
      break;
  }
}

void update_beep() {
  ++beep_ticks;
  if (beep_ticks >= beep_tick_interval) {
    beep_ticks = 0;
    beep_on = !beep_on;
    if (beep_on) {
      tone(BUZZER_PIN, 1000);
    } else {
      noTone(BUZZER_PIN);
    }
  }
}

bool load_settings() {
  int threshold;
  EEPROM.get(ADDR_THRESHOLD, threshold);

  bool valid = (EEPROM.read(ADDR_MAGIC) == EEPROM_MAGIC) && (EEPROM.read(ADDR_VERSION) == EEPROM_VERSION) && (threshold >= 1) && (threshold <= 1023);

  if (!valid) {
    EEPROM.update(ADDR_MAGIC, EEPROM_MAGIC);
    EEPROM.update(ADDR_VERSION, EEPROM_VERSION);
    light_threshold = default_ldr_threshold;
    save_threshold();
    save_state(false);
    Serial.println("EEPROM empty or corrupted, defaults saved");
    return false;
  }

  light_threshold = threshold;
  Serial.print("Loaded threshold = ");
  Serial.println(light_threshold);
  return (EEPROM.read(ADDR_STATE) == 1);
}

void save_threshold() {
  EEPROM.put(ADDR_THRESHOLD, light_threshold);
}

void save_state(bool system_on) {
  EEPROM.update(ADDR_STATE, system_on ? 1 : 0);
}