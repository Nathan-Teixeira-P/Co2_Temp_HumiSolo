/******************************************************************************

Welcome to GDB Online.
GDB online is an online compiler and debugger tool for C, C++, Python, Java, PHP, Ruby, Perl,
C#, OCaml, VB, Swift, Pascal, Fortran, Haskell, Objective-C, Assembly, HTML, CSS, JS, SQLite, Prolog.
Code, Compile, Run and Debug online from anywhere in world.

*******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define QTD 5

int main(void) {
    float co2[QTD];
    int alertas = 0;
    char nome_setor[50];

    srand(time(NULL));

    printf("=== ECOTECH MONITOR: QUALIDADE DO AR (CO2) ===\n");
    printf("Digite o nome do setor agricola: ");
    scanf(" %[^\n]", nome_setor);

    for (int i = 0; i < QTD; i++) {
        co2[i] = (rand() % 1700) + 300.0; // PPM simulado entre 300 e 2000
    }

    for (int i = 0; i < QTD; i++) {
        if (co2[i] > 1000.0) {
            alertas++;
        }
    }

    printf("\nProcessando dados do setor: %s...\n", nome_setor);
    printf("ALERTA TOXIDADE: %d sensores acima de 1000 PPM de CO2!\n", alertas);

    return 0;
}