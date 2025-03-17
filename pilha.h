#ifndef _libPilha_
#define _libPilha_

#include <stdio.h>
#include <string.h>

#define TAM_MAX 4096
#define TAM_MAX_NOME 64

typedef struct 
{
    char items[TAM_MAX][TAM_MAX_NOME];
    int topo;
} pilhaString;

void inicializa_pilha(pilhaString *pilha);

int pilha_cheia(pilhaString *pilha);

int pilha_vazia(pilhaString *pilha);

int push(pilhaString *pilha, const char *str);

int pop(pilhaString *pilha, char *buffer);

int topo(pilhaString *pilha, char *buffer);

// Função para ver se um buffer já está na pilha
int esta_na_pilha(pilhaString *pilha, char *buffer);

#endif