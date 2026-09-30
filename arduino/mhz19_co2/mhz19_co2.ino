/* ===========================================================================
 * ECOTECH MONITOR - COLETOR DE CO2 (MH-Z19B)
 * Atividade Pratica 5 - versao de demonstracao com hardware real
 *
 * ALUNO: >>> PREENCHER COM O NOME COMPLETO <<<
 *
 * O QUE ESTE SKETCH FAZ
 * Le o sensor NDIR MH-Z19B por UART e imprime UM valor de PPM por linha na
 * serial do USB. Nada mais: sem texto, sem rotulo, sem unidade. O programa em
 * C do lado do PC (ecotech_co2_sensor.c) le exatamente esse formato.
 *
 * LIGACAO (Arduino Uno / Nano)
 *   MH-Z19B Vin  -> 5V
 *   MH-Z19B GND  -> GND
 *   MH-Z19B TX   -> pino 10 (RX do SoftwareSerial)
 *   MH-Z19B RX   -> pino 11 (TX do SoftwareSerial)
 *
 * Por que SoftwareSerial: no Uno a Serial de hardware e a mesma do cabo USB.
 * Usar ela para o sensor brigaria com a comunicacao com o PC.
 *
 * AQUECIMENTO
 * O MH-Z19B precisa de cerca de 3 minutos ligado para estabilizar. Antes disso
 * os valores sobem e descem sozinhos. O sketch espera 10 s para nao travar o
 * teste; para medicao seria bom deixar aquecer mais.
 * ========================================================================= */

#include <SoftwareSerial.h>

#define PINO_RX 10  /* recebe do TX do sensor */
#define PINO_TX 11  /* envia para o RX do sensor */

#define INTERVALO_MS 2000  /* 2 s entre amostras: 24 amostras = 48 s por setor */

SoftwareSerial mhz19(PINO_RX, PINO_TX);

/* Comando de leitura de concentracao de CO2, conforme datasheet do MH-Z19B.
   Ultimo byte e o checksum do proprio comando. */
const byte CMD_LER_CO2[9] = {0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79};

/* Checksum do protocolo: 0xFF menos a soma dos bytes 1..7, mais 1. */
byte calcularChecksum(byte *pacote) {
  byte soma = 0;
  for (int i = 1; i < 8; i++) {
    soma += pacote[i];
  }
  return (byte)(0xFF - soma + 1);
}

/* Devolve a concentracao em PPM, ou -1 se a resposta veio corrompida.
   Nunca devolve valor inventado: se o checksum falha, o PC descarta. */
int lerCO2() {
  byte resposta[9];
  int recebidos = 0;

  /* limpa restos de uma leitura anterior */
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
    return -1;  /* sensor nao respondeu no prazo */
  }
  if (resposta[0] != 0xFF || resposta[1] != 0x86) {
    return -1;  /* cabecalho errado: pacote fora de sincronia */
  }
  if (resposta[8] != calcularChecksum(resposta)) {
    return -1;  /* checksum nao fecha: bytes corrompidos no caminho */
  }

  /* PPM vem em dois bytes, mais significativo primeiro */
  return resposta[2] * 256 + resposta[3];
}

void setup() {
  Serial.begin(9600);
  mhz19.begin(9600);
  delay(10000);  /* aquecimento minimo do sensor */
}

void loop() {
  int ppm = lerCO2();

  /* Imprime SO o numero. Qualquer texto extra aqui viraria ruido para o
     programa em C, que espera um numero por linha. */
  if (ppm > 0) {
    Serial.println(ppm);
  }

  delay(INTERVALO_MS);
}
