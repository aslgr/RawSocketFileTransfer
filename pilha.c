#include "pilha.h"

void inicializa_pilha(pilhaString *pilha) 
{
    pilha->topo = -1;
}

int pilha_cheia(pilhaString *pilha) 
{
    return pilha->topo == TAM_MAX - 1;
}

int pilha_vazia(pilhaString *pilha) 
{
    return pilha->topo == -1;
}

int push(pilhaString *pilha, const char *str) 
{
    if (pilha_cheia(pilha)) 
    {
        printf("pilha cheia, impossível fazer push com a string '%s'\n", str);
        return -1;
    }

    pilha->topo++;
    strncpy(pilha->items[pilha->topo], str, TAM_MAX_NOME - 1);
    // pilha->items[pilha->topo][TAM_MAX_NOME - 1] = '\0';
    return 0;
}

int pop(pilhaString *pilha, char *buffer) 
{
    if (pilha_vazia(pilha)) 
    {
        printf("pilha vazia, não da pra fazer pop\n");
        return -1;
    }

    strncpy(buffer, pilha->items[pilha->topo], TAM_MAX_NOME - 1);
    // buffer[TAM_MAX_NOME - 1] = '\0';  
    pilha->topo--;
    return 0;
}

int topo(pilhaString *pilha, char *buffer) 
{
    if (pilha_vazia(pilha)) 
    {
        printf("pilha vazia, não tem topo\n");
        return -1;
    }

    strncpy(buffer, pilha->items[pilha->topo], TAM_MAX_NOME - 1);
    // buffer[TAM_MAX_NOME - 1] = '\0';
    return 0;
}

int esta_na_pilha(pilhaString *pilha, char *buffer)
{
    for (int i = 0; i <= pilha->topo; i++) 
        if (!strcmp(buffer, pilha->items[i])) 
            return 1;
    return 0;
}

