#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/statvfs.h>

#ifndef _libServer_
#define _libServer_

#define TAM_DADOS 64 // 1byte = 8bits, 64bytes = 512bits
#define TAM_JANELA 5  // Tamanho da janela da Janela Deslizante
#define CAMINHO_VIDEOS "./videos/"
#define CAMINHO_DIR_RAIZ "/"

#define ERRO_ACESSO_NEGADO 1
#define ERRO_NAO_ENCONTRADO 2
#define ERRO_DISCO_CHEIO 3

typedef struct
{
    unsigned char marcadorInicio: 8;
    unsigned char tamanho: 6;
    unsigned char sequencia: 5;
    unsigned char tipo: 5;
    unsigned char dados[TAM_DADOS];
    unsigned char crc8: 8;

} frame_t;

// Meio de comunicar entre cliente e servidor
int cria_raw_socket(char* nome_interface_rede); 

// Preenche todo o frame com 0's
void inicializa_frame(frame_t *frame);

// Monta a mensagem baseada na estrutura do enunciado e nos parâmetros passados
frame_t monta_mensagem(unsigned char tam, unsigned char sequencia, unsigned char tipo, unsigned char* dados, int crc_flag);

// Para printar os bits de cada campo do frame
void print_bits(unsigned char byte, int num_bits, FILE *arq); 

// Imprime o frame na tela
void print_frame(frame_t *frame, FILE *arq);

// Envia um ACK através do socket
int send_ack(int sockfd, unsigned char sequencia);

// Envia um NACK através do socket
int send_nack(int sockfd, unsigned char sequencia);

// Espera pelo ACK (com timeout) e trata eventuais NACK
// Retorna 1 quando recebe o ACK, e 0 se deu problema e 2 se recebeu um ERRO
int wait_ack(int sockfd, frame_t *frame_envio, int tempo_timeout);

// Calcula o CRC-8 da mensagem e o retorna
unsigned char calcula_crc(frame_t *frame);

// Detecta erros nos dados a partir do crc
int verifica_crc(frame_t *frame);

// Analisa se a mensagem é válida e se é um ACK
int eh_ack(frame_t *frame);

// Analisa se a mensagem é válida e se é um NACK
int eh_nack(frame_t *frame);

// Analisa se a mensagem é válida e se é um LISTA
int eh_lista(frame_t *frame);

// Analisa se a mensagem é válida e se é um BAIXAR
int eh_baixar(frame_t *frame);

// Analisa se a mensagem é válida e se é um DESCRITOR ARQUIVO
int eh_desc_arq(frame_t *frame);

// Analisa se a mensagem é válida e se é um DADOS
int eh_dados(frame_t *frame);

// Analisa se a mensagem é válida e se é um ERRO
int eh_erro(frame_t *frame);

// Analisa se a mensagem é válida e se é um FIM_TX
int eh_fimtx(frame_t *frame);

// ----------Funções de mudança de tipo
void int64_para_bytes(int64_t valor, unsigned char* bytes);
int bytes_para_int(unsigned char* bytes, int tam);
void int_para_unsigned_char(int valor, unsigned char* bytes, int tam);



#endif