//include <EEPROM.h>
//define EEPROM_SIZE 16
#define RECENT_SIZE 128
#define BUTTON_CUTOFF 256
#define SUSTAIN_MIN 1000
#define SUSTAIN_MAX 3900

const uint8_t P_SOFT = A1;
const uint8_t P_SOSTENUTO = A2;
const uint8_t P_SUSTAIN = A0;

struct pedal {
  uint8_t pin;
  uint16_t last_value;
  uint16_t min_value;
  uint16_t max_value;
  uint16_t recent_values[RECENT_SIZE];
  uint16_t pointer;
  uint64_t total;
  uint16_t current;
};

pedal soft;
pedal sostenuto;
pedal sustain;

void read_pedal(pedal *p) {
  uint16_t reading = analogRead(p->pin);
  p->last_value = reading;
  p->min_value = min(p->min_value, reading);
  p->max_value = max(p->max_value, reading);
  p->total -= p->recent_values[p->pointer];
  p->total += reading;
  p->current = (uint16_t)(p->total / RECENT_SIZE);
  p->recent_values[p->pointer] = reading;
  p->pointer = (p->pointer + 1) % RECENT_SIZE;
}

void read_pedals() {
  read_pedal(&soft);
  read_pedal(&sostenuto);
  read_pedal(&sustain);
}

uint16_t calculate_value(pedal *p) {
  return (uint16_t)((uint64_t)(p->total / RECENT_SIZE));
}

/*
Pack the state of the pedals into a byte.

Bit 7 is a flag for if the soft pedal is pressed (1) or not (0).
Bit 6 is a flag for if the sostenuto pedal is pressed (1) or not (0).
Bits 5-0 are an unsigned 6-bit number indicating how pressed the sustain pedal is
(0b000000 for not at all and 0b111111 for fully pressed).
*/
uint8_t pack_data() {
  uint8_t a = calculate_value(&soft) < BUTTON_CUTOFF ? 0b10000000 : 0;
  uint8_t b = calculate_value(&sostenuto) < BUTTON_CUTOFF ? 0b01000000 : 0;
  uint16_t x = calculate_value(&sustain);
  x = x < SUSTAIN_MIN ? SUSTAIN_MIN : x;
  x = x > SUSTAIN_MAX ? SUSTAIN_MAX : x;
  float percentage = ((float)(x - SUSTAIN_MIN)) / ((float)(SUSTAIN_MAX - SUSTAIN_MIN));
  uint8_t c = (uint8_t)(percentage * 0b111111);
  return a | b | c;
}

uint8_t last_sent;

void setup() {
  //EEPROM.begin(EEPROM_SIZE);
  analogReadResolution(12);
  soft = {P_SOFT, 0, UINT16_MAX, 0, {0}, 0, 0, 0};
  sostenuto = {P_SOSTENUTO, 0, UINT16_MAX, 0, {0}, 0, 0, 0};
  sustain = {P_SUSTAIN, 0, UINT16_MAX, 0, {0}, 0, 0, 0};
  for (uint16_t i = 0; i < RECENT_SIZE; i++) {
    read_pedals();
    delayMicroseconds(10);
  }
  Serial.begin(9600);
  while (!Serial) {delay(1);}
  last_sent = pack_data();
  Serial.write(last_sent);
}

void loop() {
  read_pedals();
  uint8_t new_data = pack_data();
  if (new_data != last_sent) {
    last_sent = new_data;
    Serial.write(new_data);
  }
  while (Serial.available() > 0) {
    int incomingByte = Serial.read();
    //if (incomingByte == 65) {}
    Serial.write(new_data);
  }
  delayMicroseconds(10);
}
