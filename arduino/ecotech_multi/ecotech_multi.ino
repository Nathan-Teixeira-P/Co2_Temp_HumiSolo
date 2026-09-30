#include <SoftwareSerial.h>

#define PINO_RX 10
#define PINO_TX 11
#define PINO_SOLO A0
#define INTERVALO_MS 2000

const int SOLO_SECO = 620;
const int SOLO_MOLHADO = 310;
const int TEMP_OFFSET = 0;

SoftwareSerial mhz19(PINO_RX, PINO_TX);
const byte CMD_LER[9] = {0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79};

byte checksum(byte *p) {
  byte s = 0;
  for (int i = 1; i < 8; i++) s += p[i];
  return (byte)(0xFF - s + 1);
}

bool lerMHZ(int *ppm, int *temp) {
  byte r[9];
  int n = 0;
  while (mhz19.available()) mhz19.read();
  mhz19.write(CMD_LER, 9);
  unsigned long limite = millis() + 1000UL;
  while (n < 9 && millis() < limite) {
    if (mhz19.available()) r[n++] = mhz19.read();
  }
  if (n != 9 || r[0] != 0xFF || r[1] != 0x86 || r[8] != checksum(r)) return false;
  *ppm = r[2] * 256 + r[3];
  *temp = (int)r[4] - 40 + TEMP_OFFSET;
  return true;
}

void setup() {
  Serial.begin(9600);
  mhz19.begin(9600);
  delay(10000);
}

void loop() {
  int ppm, temp;
  if (lerMHZ(&ppm, &temp) && ppm > 0) {
    float solo = 100.0 * (SOLO_SECO - analogRead(PINO_SOLO)) / (SOLO_SECO - SOLO_MOLHADO);
    solo = constrain(solo, 0.0, 100.0);
    Serial.print(ppm); Serial.print(',');
    Serial.print(solo, 1); Serial.print(',');
    Serial.println(temp);
  }
  delay(INTERVALO_MS);
}
