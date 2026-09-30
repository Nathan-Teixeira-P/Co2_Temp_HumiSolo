/* ===========================================================================
 * ECOTECH MONITOR - MODULO CO2 COM SENSOR REAL (MH-Z19B via Arduino)
 * Atividade Pratica 5 - versao de demonstracao com hardware
 *
 * ALUNO: >>> PREENCHER COM O NOME COMPLETO <<<
 * Disciplina: Algoritmo e Estrutura de Dados
 * Professor: Me. Valdir Prado
 *
 * -------------------------------------------------------------------------
 * ATENCAO: ESTE ARQUIVO NAO E A ENTREGA DO LXP.
 * A Diretriz 2 da atividade exige rand() na main para simular o hardware, e
 * a plataforma executa o codigo em sandbox, sem porta serial. A entrega e o
 * arquivo ecotech_co2.c. Este aqui e a demonstracao com sensor real.
 * -------------------------------------------------------------------------
 *
 * COMO FUNCIONA
 * O Arduino imprime um valor de PPM por linha na serial. Este programa le
 * esses numeros da ENTRADA PADRAO e preenche a mesma matriz [SENSORES][HORAS]
 * da versao entregue. A funcao analisar_co2 e identica: mesma assinatura,
 * mesmo corpo. So a origem do dado muda - rand() virou sensor.
 *
 * COMO RODAR
 *   1) Grave arduino/mhz19_co2/mhz19_co2.ino no Arduino
 *   2) Descubra a porta:   ls /dev/ttyACM* /dev/ttyUSB*
 *   3) Ajuste a serial:    stty -F /dev/ttyACM0 9600 raw -echo
 *   4) Compile:            gcc -Wall -Wextra -std=c11 -o ecotech_sensor ecotech_co2_sensor.c
 *   5) Execute:            cat /dev/ttyACM0 | ./ecotech_sensor
 *
 * O progresso da coleta sai em stderr e o painel final em stdout, entao da
 * para salvar so o painel:
 *   cat /dev/ttyACM0 | ./ecotech_sensor > painel.txt
 *
 * TESTAR SEM O ARDUINO
 *   python3 -c "import random;[print(random.randint(400,1700)) for _ in range(72)]" | ./ecotech_sensor
 * ========================================================================= */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Mesmas dimensoes da versao entregue, para a assinatura da funcao
   especialista continuar identica. Atencao ao nome: com sensor real cada
   coluna e uma AMOSTRA (a cada 2 s), nao uma hora de relogio. O nome HORAS
   foi mantido so para preservar a paridade com ecotech_co2.c. */
#define SENSORES 1
#define HORAS 24
#define TAM_NOME 24

/* Regras de negocio identicas a versao entregue */
#define LIMITE_CO2 1000.0f
#define MEDIA_CRITICA 1300.0f

/* Faixa fisica do MH-Z19B e de 400 a 5000 PPM. Aceito uma janela mais larga
   para nao descartar leitura legitima de sensor descalibrado, mas qualquer
   coisa fora disso e ruido de serial, nao ar. */
#define PPM_MINIMO 200.0f
#define PPM_MAXIMO 10000.0f

int ler_amostra(float *ppm, int *descartadas);
int coletar_historico(float matriz[][HORAS], char nomes[][TAM_NOME], int linhas, int colunas);
void analisar_co2(float matriz[][HORAS], char nomes[][TAM_NOME], int linhas, int colunas);

int main(void) {
    /* Memoria estatica, como na versao entregue: sem malloc, sem ponteiro duplo */
    float historico_co2[SENSORES][HORAS];
    char nomes[SENSORES][TAM_NOME] = {"Estufa do tio Valdir"};

    /* Nao ha srand() nem rand() aqui: o dado vem do sensor */
    if (!coletar_historico(historico_co2, nomes, SENSORES, HORAS)) {
        fprintf(stderr, "\nERRO: coleta incompleta, o painel nao foi gerado.\n");
        fprintf(stderr, "Verifique se o Arduino esta conectado e se a porta esta correta.\n");
        return 1;
    }

    /* Mesma delegacao da versao entregue */
    analisar_co2(historico_co2, nomes, SENSORES, HORAS);

    return 0;
}

/* Le da entrada padrao a proxima amostra valida, em PPM.
   Devolve 1 e grava em *ppm; devolve 0 quando a entrada termina.
   Numero fora da faixa fisica e contado em *descartadas e ignorado; texto no
   lugar de numero (ruido de serial) e descartado silenciosamente. Em nenhum
   caso se inventa um valor para completar a matriz. */
int ler_amostra(float *ppm, int *descartadas) {
    for (;;) {
        int lidos = scanf("%f", ppm);

        if (lidos == 1) {
            if (*ppm >= PPM_MINIMO && *ppm <= PPM_MAXIMO) {
                return 1;
            }
            (*descartadas)++;
            fprintf(stderr, "  descartada, fora da faixa fisica: %.1f PPM\n", *ppm);
            continue;
        }

        if (lidos == EOF) {
            return 0;
        }

        /* lidos == 0: veio texto onde se esperava numero. scanf nao consome o
           token, entao e preciso engolir um caractere para nao travar em laco
           infinito. */
        if (getchar() == EOF) {
            return 0;
        }
    }
}

/* Preenche a matriz com leituras reais, um setor por linha.
   Devolve 1 se completou a matriz, 0 se a entrada acabou antes. */
int coletar_historico(float matriz[][HORAS], char nomes[][TAM_NOME], int linhas, int colunas) {
    int descartadas = 0;
    int total = linhas * colunas;

    fprintf(stderr, "=== ECOTECH MONITOR: COLETA COM SENSOR REAL (MH-Z19B) ===\n");
    fprintf(stderr, "%d estufa(s) x %d amostras = %d leituras\n", linhas, colunas, total);
    fprintf(stderr, "A 2 s por amostra, cerca de %d s por setor.\n\n", colunas * 2);

    for (int i = 0; i < linhas; i++) {
        fprintf(stderr, ">>> POSICIONE O SENSOR EM: %s\n", nomes[i]);

        for (int j = 0; j < colunas; j++) {
            if (!ler_amostra(&matriz[i][j], &descartadas)) {
                fprintf(stderr, "\nA entrada terminou com %d de %d leituras.\n",
                        i * colunas + j, total);
                return 0;
            }

            fprintf(stderr, "  [%-22s] amostra %2d/%d: %7.1f PPM\n",
                    nomes[i], j + 1, colunas, matriz[i][j]);
        }

        fprintf(stderr, "  setor %s concluido\n\n", nomes[i]);
    }

    if (descartadas > 0) {
        fprintf(stderr, "Leituras descartadas por estarem fora da faixa: %d\n", descartadas);
    }
    fprintf(stderr, "Coleta completa. Gerando painel...\n\n");

    return 1;
}

/* Funcao especialista - identica a de ecotech_co2.c, de proposito.
   Recebe a matriz numerica E a matriz de textos simultaneamente, e nao sabe
   de onde o dado veio. Trocar rand() por sensor real nao exigiu mudar uma
   linha dela: e o ganho da arquitetura que a atividade pede.
   Unica diferenca de saida: o rotulo diz "amostra" em vez de "hora", porque
   aqui as colunas sao amostras de 2 s, nao horas de relogio. */
void analisar_co2(float matriz[][HORAS], char nomes[][TAM_NOME], int linhas, int colunas) {
    /* Rastreamento de estado triplo: valor + linha + coluna.
       Inicializado com matriz[0][0], nunca com 0.0f. No eixo CO2 o pior caso
       e o MAIOR valor (ar toxico). */
    float maior_co2 = matriz[0][0];
    int pos_sensor = 0;
    int pos_amostra = 0;

    float pior_media = 0.0f;
    int pior_setor = 0;

    char status[10];

    printf("=== ECOTECH MONITOR: QUALIDADE DO AR (CO2) ===\n");
    printf("Leitura real - sensor MH-Z19B via Arduino\n");
    printf("Historico coletado: %d estufa(s) x %d amostras (PPM)\n\n", linhas, colunas);

    printf("%-22s %12s %14s   %s\n", "SETOR", "MEDIA (PPM)", "ACIMA DE 1000", "STATUS");
    printf("-----------------------------------------------------------\n");

    for (int i = 0; i < linhas; i++) {
        /* ARMADILHA DO ACUMULADOR: soma zerada AQUI, dentro do laco externo,
           uma vez por setor. Fora dele, cada setor herdaria o CO2 do anterior. */
        float soma = 0.0f;
        int leituras_toxicas = 0;
        float media;

        for (int j = 0; j < colunas; j++) {
            soma += matriz[i][j];

            if (matriz[i][j] > LIMITE_CO2) {
                leituras_toxicas++;
            }

            /* As tres variaveis do pico sao atualizadas juntas */
            if (matriz[i][j] > maior_co2) {
                maior_co2 = matriz[i][j];
                pos_sensor = i;
                pos_amostra = j;
            }
        }

        media = soma / colunas;

        if (media >= MEDIA_CRITICA) {
            strcpy(status, "CRITICO");
        } else if (media >= LIMITE_CO2) {
            strcpy(status, "ALERTA");
        } else {
            strcpy(status, "IDEAL");
        }

        /* Painel humanizado: imprime nomes[i], nunca o indice i */
        printf("%-22s %12.2f %14d   %s\n", nomes[i], media, leituras_toxicas, status);

        if (i == 0 || media > pior_media) {
            pior_media = media;
            pior_setor = i;
        }
    }

    printf("-----------------------------------------------------------\n");

    printf("\nALERTA DE TOXIDADE: pico de %.1f PPM em %s (amostra %d)\n",
           maior_co2, nomes[pos_sensor], pos_amostra);
    printf("SETOR MAIS COMPROMETIDO: %s, com media de %.2f PPM\n",
           nomes[pior_setor], pior_media);

    /* A recomendacao acompanha o setor de MAIOR MEDIA, o mesmo da linha acima,
       e nao o setor do pico isolado - senao o painel se contradiz. */
    if (pior_media >= MEDIA_CRITICA) {
        printf("RECOMENDACAO: acionar exaustao imediata em %s\n", nomes[pior_setor]);
    } else {
        printf("RECOMENDACAO: manter ventilacao em regime normal\n");
    }
}
