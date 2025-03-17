#include "libServer.h"

int cria_raw_socket(char* nome_interface_rede) 
{ 
    // Cria o Raw Socket
    int soquete = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (soquete == -1) 
    { 
        fprintf(stderr, "Erro ao criar socket: Verifique se você é root!\n");
        exit(-1); 
    }
    printf("Soquete criado\n");

    int ifindex = if_nametoindex(nome_interface_rede);

    struct sockaddr_ll endereco = {0};
    endereco.sll_family = AF_PACKET;
    endereco.sll_protocol = htons(ETH_P_ALL);
    endereco.sll_ifindex = ifindex;

    // Bind the socket to the network interface
    if (bind(soquete, (struct sockaddr *)&endereco, sizeof(endereco)) == -1)
    {
        perror("Erro ao fazer bind no socket\n");
        exit(-1);
    }

    struct packet_mreq mr = {0};
    mr.mr_ifindex = ifindex;
    mr.mr_type = PACKET_MR_PROMISC;

    // Coloca o socket no modo promíscuo (Não joga fora o que identifica como lixo)
    if (setsockopt(soquete, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mr, sizeof(mr)) == -1) 
    {
        fprintf(stderr, "Erro ao fazer setsockopt: "
            "Verifique se a interface de rede foi especificada corretamente.\n");
        exit(-1);
    }

    return soquete;
}

void inicializa_frame(frame_t *frame) 
{
    memset(frame, 0, sizeof(frame_t));
    memset(frame->dados, 0, TAM_DADOS);
}

frame_t monta_mensagem(unsigned char tam, unsigned char sequencia, unsigned char tipo, unsigned char* dados, int crc_flag)
{
    frame_t frame;
    
    frame.marcadorInicio = 0x7E; // (0111 1110)
    frame.tamanho = tam;
    frame.sequencia = sequencia;
    frame.tipo = tipo;

    if (dados != NULL)
        memcpy(frame.dados, dados, tam);

    // caso seja necessário, calcula o crc
    if (crc_flag){
        frame.crc8 = calcula_crc(&frame);
    } else {
        frame.crc8 = 0x00;
    }

    return frame;
}

// Função interna para imprimir os bits de cada byte
void print_bits(unsigned char byte, int num_bits, FILE *arq) 
{
    for (int i = num_bits - 1; i >= 0; --i)
        fprintf(arq, "%d", (byte >> i) & 1);
}

void print_frame(frame_t *frame, FILE *arq) 
{
    fprintf(arq, "\nMarcador de inicio: ");
    print_bits(frame->marcadorInicio, 8, arq);
    fprintf(arq, "\n");

    fprintf(arq, "Tamanho: ");
    print_bits(frame->tamanho, 6, arq);
    fprintf(arq, "\n");

    fprintf(arq, "Sequência: ");
    print_bits(frame->sequencia, 5, arq);
    fprintf(arq, "\n");

    fprintf(arq, "Tipo: ");
    print_bits(frame->tipo, 5, arq);
    fprintf(arq, "\n");

    fprintf(arq, "Dados: ");
    for (int i = 0; i < TAM_DADOS-1; ++i) {
        print_bits(frame->dados[i], 8, arq);
    }
    fprintf(arq, "\n");

    fprintf(arq, "Crc-8: ");
    print_bits(frame->crc8, 8, arq);
    fprintf(arq, "\n\n");
}

int send_ack(int sockfd, unsigned char sequencia)
{
    frame_t frame;
    unsigned char dados[TAM_DADOS];

    // Prepara a mensagem de volta (ACK)
    memset(dados, 0, TAM_DADOS);
    frame = monta_mensagem(0x00, sequencia, 0x00, dados, 0);

    // Enviando ACK
    if (send(sockfd, &frame, sizeof(frame), 0) < 0)
    {
        perror("Erro no envio:");
        return 0;
    }
    
    return 1;
}

int send_nack(int sockfd, unsigned char sequencia)
{
    frame_t frame;
    unsigned char dados[TAM_DADOS];

    // Prepara a mensagem de volta (NACK)
    memset(dados, 0, TAM_DADOS);
    frame = monta_mensagem(0x00, sequencia, 0x01, dados, 0);

    // Enviando NACK
    if (send(sockfd, &frame, sizeof(frame), 0) < 0)
    {
        perror("Erro no envio:");
        return 0;
    }

    return 1;
}

// Retorna o tempo atual em milissegundos
long long timestamp()
{
    struct timeval tp;
    gettimeofday(&tp, NULL);
    return (long long)tp.tv_sec * 1000 + tp.tv_usec / 1000;
}

int wait_ack(int sockfd, frame_t *frame_envio, int tempo_timeout)
{
    frame_t frame_recebimento;
    long long marcador_tempo = 0, tempo_inicial = timestamp();

    // Aguarda pelo recebimento do ACK do server
    if (recv(sockfd, &frame_recebimento, sizeof(frame_recebimento), 0) < 0)
    {
        perror("Erro no recebimento:");
        return 0;
    }

    if (eh_erro(&frame_recebimento))
        return 2;

    while (!(eh_ack(&frame_recebimento) && frame_recebimento.sequencia == frame_envio->sequencia) && (marcador_tempo = timestamp() - tempo_inicial) < tempo_timeout)
    {
        if (eh_nack(&frame_recebimento) && frame_recebimento.sequencia == frame_envio->sequencia)
        {
            if (send(sockfd, frame_envio, sizeof(*frame_envio), 0) < 0)
            {
                perror("Erro no envio:");
                return 0;
            }

            // Reinicia o timeout
            tempo_inicial = timestamp();
        }

        // Situações decorrentes do não recebimento do ACK pela contraparte:
        // -> Client pede lista, server manda o ACK mas client não recebe...
        // -> Server envia um fim_tx, client manda o ACK mas server não recebe...
        // -> Client pede para baixar, server manda o ACK mas client não recebe...
        if (eh_lista(&frame_recebimento) || eh_fimtx(&frame_recebimento) || (eh_baixar(&frame_recebimento) && eh_dados(frame_envio)))
        {
            if (frame_envio->sequencia == 0)
            {
                if (!send_ack(sockfd, (unsigned char)31))
                    return 0;
            }
            else
            {
                if (!send_ack(sockfd, frame_envio->sequencia - 1))
                    return 0;
            }

            if (send(sockfd, frame_envio, sizeof(*frame_envio), 0) < 0)
            {
                perror("Erro no envio:");
                return 0;
            }

            // Reinicia o timeout
            tempo_inicial = timestamp();
        }

        if (recv(sockfd, &frame_recebimento, sizeof(frame_recebimento), 0) < 0)
        {
            perror("Erro no recebimento:");
            return 0;
        }

        if (eh_erro(&frame_recebimento))
            return 2;
    }

    // Caso tenha atingido o timeout, reenvia a msg e recursivamente espera pelo ACK
    if (marcador_tempo >= tempo_timeout)
    {
        if (send(sockfd, frame_envio, sizeof(*frame_envio), 0) < 0)
        {
            perror("Erro no envio:");
            return 0;
        }

        if (!wait_ack(sockfd, frame_envio, tempo_timeout * 2))
            return 0;
    }

    return 1;
}

unsigned char calcula_crc(frame_t *frame)
{
    unsigned char gerador = 0x07; // 8 bits, porque o bit mais significativo é implicitamente 1
    unsigned char crc = 0x00; // inicialmente é 0;

    for (unsigned int i = 0; i < frame->tamanho; i++)
    {
        crc ^= frame->dados[i];

        for (unsigned int j = 0; j < 8; j++)
        {
            if (crc & 0x80){
                crc = (crc << 1) ^ gerador;
            } else {
                crc <<= 1;
            }
        }
    }

    return crc;
}

int verifica_crc(frame_t *frame)
{
    if (frame->crc8 == calcula_crc(frame))
        return 1;
    
    return 0;
}

int eh_ack(frame_t *frame)
{
    if (frame->marcadorInicio != 0x7E || frame->tipo != 0x00)
        return 0;
    return 1;
}

int eh_nack(frame_t *frame)
{
    if (frame->marcadorInicio != 0x7E || frame->tipo != 0x01)
        return 0;
    return 1;
}

int eh_lista(frame_t *frame)
{
    if (frame->marcadorInicio != 0x7E || frame->tipo != 0x0A)
        return 0;
    return 1;
}

int eh_baixar(frame_t *frame)
{
    if (frame->marcadorInicio != 0x7E || frame->tipo != 0x0B)
        return 0;
    return 1;
}

int eh_desc_arq(frame_t *frame)
{
    if (frame->marcadorInicio != 0x7E || frame->tipo != 0x11)
        return 0;
    return 1;
}

int eh_dados(frame_t *frame)
{
    if (frame->marcadorInicio != 0x7E || frame->tipo != 0x12)
        return 0;
    return 1;
}

int eh_erro(frame_t *frame)
{
    if (frame->marcadorInicio != 0x7E || frame->tipo != 0x1F)
        return 0;
    return 1;
}

int eh_fimtx(frame_t *frame)
{
    if (frame->marcadorInicio != 0x7E || frame->tipo != 0x1E)
        return 0;
    return 1;
}

void int64_para_bytes(int64_t valor, unsigned char* bytes) 
{
    for (int i = 0; i < 8; i++)
        bytes[i] = (unsigned char)((valor >> (i * 8)) & 0xFF);
}

int bytes_para_int(unsigned char* bytes, int tam) 
{
    int valor = 0;
    for (int i = 0; i < tam; i++)
        valor |= (int)bytes[i] << (i * 8);

    return valor;
}

void int_para_unsigned_char(int valor, unsigned char* bytes, int tam) 
{
    for (int i = 0; i < tam; i++)
        bytes[i] = (valor >> (i * 8)) & 0xFF;
}
