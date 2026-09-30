/* ===========================================================================
 * ECOTECH MONITOR - MODULO CO2 (QUALIDADE DO AR)
 * Atividade Pratica 5 - Evolucao Inovatech:
 * Orquestrador Centralizado e Painel Humanizado
 *
 * ALUNO: >>> PREENCHER COM O NOME COMPLETO ANTES DE ENVIAR NO LXP <<<
 * Disciplina: Algoritmo e Estrutura de Dados
 * Professor: Me. Valdir Prado
 * Aula 07 - Vetores Bidimensionais (Matrizes)
 *
 * Evolucao do legado 1D (vetor float co2[5], contagem global de leituras
 * acima de 1000 PPM) para uma matriz temporal 2D [SENSORES][HORAS], com
 * painel que identifica cada setor pelo nome, nunca pelo indice.
 * ========================================================================= */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

/* Diretriz 1: dimensoes fixas, sem alocacao dinamica */
#define SENSORES 3
#define HORAS 24
#define TAM_NOME 20

/* Regra de negocio do eixo CO2 (limiar de 1000 PPM herdado do legado).
   Sao duas grandezas diferentes e nao devem compartilhar o mesmo numero:
   LIMITE_CO2   -> aplicado a UMA leitura isolada;
   MEDIA_CRITICA -> aplicado a MEDIA das 24 leituras do setor.
   A media de 24 sorteios em [300, 2000] se concentra perto de 1150 PPM, entao
   exigir 1500 dela tornaria o status CRITICO praticamente inalcancavel. */
#define LIMITE_CO2 1000.0f
#define MEDIA_CRITICA 1300.0f

/* Diretriz 3: prototipo declarado antes da main.
   A segunda dimensao [HORAS] e obrigatoria para o compilador calcular o
   salto de memoria de uma linha para a outra. A funcao recebe a matriz
   numerica E a matriz de textos simultaneamente. */
void analisar_co2(float matriz[][HORAS], char nomes[][TAM_NOME], int linhas, int colunas);

int main(void) {
    /* Diretriz 1 / Diretriz 5: as duas matrizes sao declaradas aqui na main,
       em memoria estatica e contigua. Nenhuma variavel global. */
    float historico_co2[SENSORES][HORAS];
    char nomes[SENSORES][TAM_NOME] = {"Estufa Alpha", "Canteiro Sul", "Setor Irrigado"};

    /* Diretriz 2: semente sorteada uma unica vez, no topo da main.
       Jamais dentro da funcao de analise (senao os valores se repetem). */
    srand(time(NULL));

    /* Diretriz 2: esteira de entrada com lacos aninhados na propria main.
       Telemetria simulada de 300.0 a 2000.0 PPM - sem scanf, o hardware
       e simulado. O legado usava % 1700 e nunca chegava aos 2000 PPM. */
    for (int i = 0; i < SENSORES; i++) {
        for (int j = 0; j < HORAS; j++) {
            historico_co2[i][j] = (rand() % 1701) + 300.0f;
        }
    }

    /* Diretriz 3: a main apenas orquestra e delega para a especialista */
    analisar_co2(historico_co2, nomes, SENSORES, HORAS);

    return 0;
}

/* Diretriz 3: funcao especialista do eixo CO2.
   So le o que a main preencheu - nao usa rand() nem srand(). */
void analisar_co2(float matriz[][HORAS], char nomes[][TAM_NOME], int linhas, int colunas) {
    /* Rastreamento de estado triplo (slide 6): valor + linha + coluna.
       Inicializado com matriz[0][0], nunca com 0.0f - se o ar estivesse
       todo abaixo de zero (ou se o pico fosse o proprio primeiro valor),
       comecar em 0.0f apontaria para um pico que nao existe.
       No eixo CO2 o pior caso e o MAIOR valor (ar toxico); no eixo Solo
       seria o menor (seca) - mesma tecnica, comparacao invertida. */
    float maior_co2 = matriz[0][0];
    int pos_sensor = 0;
    int pos_hora = 0;

    /* Resumo final: qual setor esta pior no eixo CO2 */
    float pior_media = 0.0f;
    int pior_setor = 0;

    /* Slide 7: status textual montado com strcpy */
    char status[10];

    printf("=== ECOTECH MONITOR: QUALIDADE DO AR (CO2) ===\n");
    printf("Atividade Pratica 5 - Painel Humanizado por Setor\n");
    printf("Historico simulado: %d setores x %d horas (PPM)\n\n", linhas, colunas);

    /* Slide 7: cabecalho da tabela alinhado com %-15s */
    printf("%-15s %12s %14s   %s\n", "SETOR", "MEDIA (PPM)", "ACIMA DE 1000", "STATUS");
    printf("----------------------------------------------------\n");

    for (int i = 0; i < linhas; i++) {
        /* ARMADILHA DO ACUMULADOR (slide 6): soma e zerada AQUI, dentro do
           laco externo, uma vez por setor. Declarada fora do laco, o setor
           seguinte herdaria o CO2 do anterior e todas as medias sairiam
           corrompidas para cima. */
        float soma = 0.0f;
        int leituras_toxicas = 0;
        float media;

        for (int j = 0; j < colunas; j++) {
            soma += matriz[i][j];

            /* Contagem de toxidade por setor - evolucao do legado, que
               contava apenas o total global sem separar por sensor */
            if (matriz[i][j] > LIMITE_CO2) {
                leituras_toxicas++;
            }

            /* As tres variaveis do pico sao atualizadas juntas */
            if (matriz[i][j] > maior_co2) {
                maior_co2 = matriz[i][j];
                pos_sensor = i;
                pos_hora = j;
            }
        }

        media = soma / colunas;

        /* Slide 7: strcpy para o status textual, decidido pela media */
        if (media >= MEDIA_CRITICA) {
            strcpy(status, "CRITICO");
        } else if (media >= LIMITE_CO2) {
            strcpy(status, "ALERTA");
        } else {
            strcpy(status, "IDEAL");
        }

        /* Diretriz 4: painel humanizado - imprime nomes[i], nunca o indice i */
        printf("%-15s %12.2f %14d   %s\n", nomes[i], media, leituras_toxicas, status);

        if (i == 0 || media > pior_media) {
            pior_media = media;
            pior_setor = i;
        }
    }

    printf("----------------------------------------------------\n");

    /* Diretriz 4: alerta final cruzando a coordenada matematica (hora) com a
       identidade real do local de coleta (nomes[pos_sensor]) */
    printf("\nALERTA DE TOXIDADE: pico de %.1f PPM em %s (hora %d)\n",
           maior_co2, nomes[pos_sensor], pos_hora);
    printf("SETOR MAIS COMPROMETIDO: %s, com media de %.2f PPM\n",
           nomes[pior_setor], pior_media);

    /* A recomendacao acompanha o setor de MAIOR MEDIA - o mesmo nomeado na linha
       acima - e nao o setor do pico isolado. Usar o pico faria o painel mandar
       ventilar um setor que ele proprio acabou de dizer que nao e o mais
       comprometido, e a condicao sobre o pico seria sempre verdadeira. */
    if (pior_media >= MEDIA_CRITICA) {
        printf("RECOMENDACAO: acionar exaustao imediata em %s\n", nomes[pior_setor]);
    } else {
        printf("RECOMENDACAO: manter ventilacao em regime normal\n");
    }
}
