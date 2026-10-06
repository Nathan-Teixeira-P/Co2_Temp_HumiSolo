#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define SENSORES 3
#define HORAS 24
#define TAM_NOME 20

#define LIMITE_CO2 1000.0f
#define MEDIA_CRITICA 1300.0f

void analisar_co2(float matriz[][HORAS], char nomes[][TAM_NOME], int linhas, int colunas);

int main(void) {
    float historico_co2[SENSORES][HORAS];
    char nomes[SENSORES][TAM_NOME] = {"Estufa Alpha", "Canteiro Sul", "Setor Irrigado"};

    srand(time(NULL));

    for (int i = 0; i < SENSORES; i++) {
        for (int j = 0; j < HORAS; j++) {
            historico_co2[i][j] = (rand() % 1701) + 300.0f;
        }
    }

    analisar_co2(historico_co2, nomes, SENSORES, HORAS);

    return 0;
}

void analisar_co2(float matriz[][HORAS], char nomes[][TAM_NOME], int linhas, int colunas) {
    float maior_co2 = matriz[0][0];
    int pos_sensor = 0;
    int pos_hora = 0;

    float pior_media = 0.0f;
    int pior_setor = 0;

    char status[10];

    printf("=== ECOTECH MONITOR: QUALIDADE DO AR (CO2) ===\n");
    printf("Atividade Pratica 5 - Painel Humanizado por Setor\n");
    printf("Historico simulado: %d setores x %d horas (PPM)\n\n", linhas, colunas);

    printf("%-15s %12s %14s   %s\n", "SETOR", "MEDIA (PPM)", "ACIMA DE 1000", "STATUS");
    printf("----------------------------------------------------\n");

    for (int i = 0; i < linhas; i++) {
        float soma = 0.0f;
        int leituras_toxicas = 0;
        float media;

        for (int j = 0; j < colunas; j++) {
            soma += matriz[i][j];

            if (matriz[i][j] > LIMITE_CO2) {
                leituras_toxicas++;
            }

            if (matriz[i][j] > maior_co2) {
                maior_co2 = matriz[i][j];
                pos_sensor = i;
                pos_hora = j;
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

        printf("%-15s %12.2f %14d   %s\n", nomes[i], media, leituras_toxicas, status);

        if (i == 0 || media > pior_media) {
            pior_media = media;
            pior_setor = i;
        }
    }

    printf("----------------------------------------------------\n");

    printf("\nALERTA DE TOXIDADE: pico de %.1f PPM em %s (hora %d)\n",
           maior_co2, nomes[pos_sensor], pos_hora);
    printf("SETOR MAIS COMPROMETIDO: %s, com media de %.2f PPM\n",
           nomes[pior_setor], pior_media);

    if (pior_media >= MEDIA_CRITICA) {
        printf("RECOMENDACAO: acionar exaustao imediata em %s\n", nomes[pior_setor]);
    } else {
        printf("RECOMENDACAO: manter ventilacao em regime normal\n");
    }
}
