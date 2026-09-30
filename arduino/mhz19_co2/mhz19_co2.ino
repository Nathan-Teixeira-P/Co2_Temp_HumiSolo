#include <SoftwareSerial.h>

#define PINO_RX 10
#define PINO_TX 11

#define INTERVALO_MS 2000

SoftwareSerial mhz19(PINO_RX, PINO_TX);

const byte CMD_LER_CO2[9] = {0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79};

byte calcularChecksum(byte *pacote) {
  byte soma = 0;
  for (int i = 1; i < 8; i++) {
    soma += pacote[i];
  }
  return (byte)(0xFF - soma + 1);
}

int lerCO2() {
  byte resposta[9];
  int recebidos = 0;

  while (mhz19.available()) {
    mhz19.read();
  }

  mhz19.write(CMD_LER_CO2, 9);

  unsigned long limite = millis() + 1000UL;
  while (recebidos < 9 && millis() < limite) {
    if (mhz19.available()) {
      resposta[recebidos] = mhz19.read();
      recebidos++;
    }
  }

  if (recebidos != 9) {
    return -1;
  }
  if (resposta[0] != 0xFF || resposta[1] != 0x86) {
    return -1;
  }
  if (resposta[8] != calcularChecksum(resposta)) {
    return -1;
  }

  return resposta[2] * 256 + resposta[3];
}

void setup() {
  Serial.begin(9600);
  mhz19.begin(9600);
  delay(10000);
}

void loop() {
  int ppm = lerCO2();

  if (ppm > 0) {
    Serial.println(ppm);
  }

  delay(INTERVALO_MS);
}
