#include "libServer.h"
#include "pilha.h"

// Função auxiliar pra remover a extensão do arquivo do nome dos vídeos
char *remove_ext(char *nome) 
{
    char *string = nome;
    // char *ponto = strrchr(string, '.');
    // if (ponto != NULL)
    //     *ponto = '\0';

    return string;
}

void print_cabecalho_client() 
{
    printf(".__________________________.\n");
    printf("|                          |\n");
    printf("|  ____    ____   ____     |       Raw Socket Streaming\n");
    printf("| |  _ \\  / ___| / ___|    |       Desenvolvido por:\n");
    printf("| | |_) | \\___ \\ \\___ \\    |       Pedro Amaral Chapelin e Arthur Lima Gropp\n");
    printf("| |  _ <   ___) | ___) |   |\n");
    printf("| |_| \\_\\ |____/ |____/    |\n");
    printf("|                          |\n");
    printf("|__________________________|\n");
}

void limpa_tela()
{
    system("clear");
    print_cabecalho_client();
}

unsigned long long pega_espaco_disco(const char* caminho) 
{
    struct statvfs stat;

    if (statvfs(caminho, &stat) != 0)
    {
        perror("statvfs deu errado");
        exit(EXIT_FAILURE);
    }

    // Calcula espaço em bytes
    unsigned long long espaco_disponivel = stat.f_bavail * stat.f_frsize;
    return espaco_disponivel;
}

int main(int argc, char *argv[]) 
{
    int erroDisco = 0, outroErro = 0;
    int sockfd, entradaOpcao, entradaVideo;
    int tamVideo, tamRecebido, versaoVideoRecebida;
    unsigned char dados[TAM_DADOS], sequencia = 0;
    char versaoVideo[32];
    unsigned long long espacoDispDisco = pega_espaco_disco(CAMINHO_DIR_RAIZ);

    FILE *arq, *arquivoTesteCliente;

    frame_t frame;
    frame_t frame_resp;
    inicializa_frame(&frame);
    inicializa_frame(&frame_resp);
   
    pilhaString pilhaFilmes;
    inicializa_pilha(&pilhaFilmes);

    printf("Iniciando Client ...\n");

    // Criação do Raw Socket para comunicação
    sockfd = cria_raw_socket("lo");
    if (sockfd == -1) 
    {
        perror("Error on client socket creation:");
        return EXIT_FAILURE;
    }

    limpa_tela();

    // INICIA CLIENT E MOSTRA OS FILMES DISPONÍVEIS

    memset(dados, 0, TAM_DADOS);
    frame = monta_mensagem(0x00, sequencia, 0x0A, dados, 0);
    sequencia = (sequencia + 1) % 32;

    // Envia o frame com tipo == LISTA
    if (send(sockfd, &frame, sizeof(frame), 0) < 0)
    {
        perror("Erro no envio:");
        return EXIT_FAILURE;
    }

    if (wait_ack(sockfd, &frame, 1000) == 0)
        return EXIT_FAILURE;

    // Começa a receber os dados referentes ao comando "lista"
    if (recv(sockfd, &frame_resp, sizeof(frame_resp), 0) < 0)
    {
        perror("Erro no recebimento:");
        return EXIT_FAILURE;
    }

    // Até receber um frame válido do tipo == "FIM_TX"
    while (!eh_fimtx(&frame_resp)) 
    {
        if (eh_dados(&frame_resp))
        {
            if (frame_resp.sequencia == sequencia)
            {
                if (verifica_crc(&frame_resp))
                {
                    if (!esta_na_pilha(&pilhaFilmes, (char*)frame_resp.dados))
                        push(&pilhaFilmes, (char*)frame_resp.dados);
                    
                    if (!send_ack(sockfd, sequencia))
                        return EXIT_FAILURE;

                    sequencia = (sequencia + 1) % 32;
                }
                else
                {
                    if (!send_nack(sockfd, sequencia))
                        return EXIT_FAILURE;
                }
            }
        }

        if (recv(sockfd, &frame_resp, sizeof(frame_resp), 0) < 0)
        {
            perror("Erro no recebimento:");
            return EXIT_FAILURE;
        }
    }

    // Frame indicando final de transmissão recebido do server.

    if (!send_ack(sockfd, sequencia))
        return EXIT_FAILURE;

    sequencia = (sequencia + 1) % 32;

    // LOOP PRINCIPAL DO CLIENT
    while (1) 
    {
        printf("\nCatálogo de vídeos disponíveis:\n\n");

        for (int i = 0; i <= pilhaFilmes.topo; i++)
            printf("[%d] %s\n", i + 1, remove_ext(pilhaFilmes.items[i]));

        printf("\nOpções:\n\n[1]. Baixar algum vídeo\n[2]. Fechar o Client\n");
        printf("\nEscolha uma opção: ");
        scanf("%d", &entradaOpcao);

        // Caso entrada inválida
        while (entradaOpcao < 1 || entradaOpcao > 2)
        {
            limpa_tela();
            printf("\nCatálogo de vídeos disponíveis:\n\n");

            for (int i = 0; i <= pilhaFilmes.topo; i++)
                printf("[%d] %s\n", i + 1, remove_ext(pilhaFilmes.items[i]));

            printf("\nEntrada inválida, escolha entre [1] e [2]\n");
            printf("\nOpções:\n\n[1]. Baixar algum filme\n[2]. Fechar o Client\n");

            printf("\nEscolha uma opção: ");
            scanf("%d", &entradaOpcao);
        }

        if (entradaOpcao == 1)
        {
            limpa_tela();
            printf("\nCatálogo de vídeos disponíveis:\n\n");

            for (int i = 0; i <= pilhaFilmes.topo; i++)
                printf("[%d] %s\n", i + 1, remove_ext(pilhaFilmes.items[i]));

            printf("\nEscolha um vídeo para baixar: ");
            scanf("%d", &entradaVideo);

            while (entradaVideo < 1 || entradaVideo > pilhaFilmes.topo + 1)
            {
                limpa_tela();
                printf("\nPor favor, escolha uma opção válida!\n");
                printf("\nCatálogo de vídeos disponíveis:\n\n");

                for (int i = 0; i <= pilhaFilmes.topo; i++)
                    printf("[%d] %s\n", i + 1, remove_ext(pilhaFilmes.items[i]));

                printf("\nEscolha uma opção: ");
                scanf("%d", &entradaVideo);
            }

            // Prepara e envia o frame com o nome do vídeo
            strncpy((char*)frame.dados, pilhaFilmes.items[entradaVideo-1], TAM_DADOS-1);

            frame.marcadorInicio = 0x7E;
            frame.tamanho = (unsigned char)strlen(pilhaFilmes.items[entradaVideo-1]);
            frame.sequencia = sequencia;
            frame.tipo = 0x0B;
            frame.crc8 = calcula_crc(&frame);

            sequencia = (sequencia + 1) % 32;

            if (send(sockfd, &frame, sizeof(frame), 0) < 0)
            {
                perror("Erro no envio:");
                return EXIT_FAILURE;
            }

            if (wait_ack(sockfd, &frame, 1000) == 0)
                return EXIT_FAILURE;

            // Cria o arquivo que será o vídeo
            arq = fopen(pilhaFilmes.items[entradaVideo-1], "wb");
            if (!arq)
            {
                perror("Erro ao abrir/criar o arquivo");
                return EXIT_FAILURE;
            }

            arquivoTesteCliente = fopen("ArquivoTesteCliente", "wb");
            if (!arquivoTesteCliente)
            {
                perror("Erro ao abrir/criar o arquivo");
                return EXIT_FAILURE;
            }

            // Começa a receber os dados referentes ao vídeo pedido
            if (recv(sockfd, &frame_resp, sizeof(frame_resp), 0) < 0)
            {
                perror("Erro no recebimento:");
                return EXIT_FAILURE;
            }

            tamRecebido = 0;
            versaoVideoRecebida = 0;

            while (!eh_fimtx(&frame_resp))
            {
                if (eh_erro(&frame_resp))
                {
                    outroErro = 1;

                    // Manda ACK
                    if (!send_ack(sockfd, sequencia))
                        return EXIT_FAILURE;
                    sequencia = (sequencia + 1) % 32;

                    if (bytes_para_int(frame_resp.dados, frame_resp.tamanho) == ERRO_ACESSO_NEGADO)
                        printf("ERRO: Permissões insuficientes para baixar o vídeo.\nVoltando à tela inicial...\n\n");
                    else if (bytes_para_int(frame_resp.dados, frame_resp.tamanho) == ERRO_NAO_ENCONTRADO)
                        printf("ERRO: Vídeo não encontrado na base.\nVoltando à tela inicial...\n\n");
                    
                    break;
                }

                else if (eh_desc_arq(&frame_resp))
                {
                    if (frame_resp.sequencia == sequencia)
                    {
                        if (verifica_crc(&frame_resp))
                        {
                            // Manda ACK
                            if (!send_ack(sockfd, sequencia))
                                return EXIT_FAILURE;
                            sequencia = (sequencia + 1) % 32;

                            print_frame(&frame_resp, arquivoTesteCliente);

                            if (!versaoVideoRecebida && tamRecebido)
                            {
                                strcpy(versaoVideo, (char*)frame_resp.dados);
                                versaoVideoRecebida = 1;

                                // Verifica se o video cabe na maquina do client
                                if (tamVideo < espacoDispDisco)
                                    printf("Iniciando download de %s...\nTamanho: %d bytes\nÚltima data de modificação: %s\n", pilhaFilmes.items[entradaVideo-1], tamVideo, versaoVideo);
                                else
                                {
                                    erroDisco = 1;
                                    sequencia = (sequencia + 1) % 32;

                                    printf("ERRO: Impossível fazer o Download porque o vídeo não cabe no disco, libere espaço e tente baixar novamente:\n");
                                    printf("Tamanho do vídeo: %d bytes\n", tamVideo);
                                    printf("Espaço disponível em disco: %llu\n", espacoDispDisco);
                                    printf("Voltando a tela inicial...\n\n");

                                    unsigned char bytes[4];
                                    int_para_unsigned_char(ERRO_DISCO_CHEIO, bytes, sizeof(int));

                                    // Prepara frame de ERRO
                                    frame = monta_mensagem(sizeof(bytes), sequencia, 0x1F, bytes, 1);
                                    sequencia = (sequencia + 1) % 32;

                                    // Envia frame de ERRO
                                    if (send(sockfd, &frame, sizeof(frame), 0) < 0)
                                    {
                                        perror("Erro no envio:");
                                        return EXIT_FAILURE;
                                    }

                                    // Espera pelo ACK
                                    if (wait_ack(sockfd, &frame, 1000) == 0)
                                            return EXIT_FAILURE;

                                    break;
                                }
                            }

                            if (!tamRecebido)
                            {
                                tamVideo = bytes_para_int(frame_resp.dados, frame_resp.tamanho);
                                tamRecebido = 1;
                            }
                        }
                        else
                        {
                            // Manda NACK
                            if (!send_nack(sockfd, sequencia))
                                return EXIT_FAILURE;
                        }
                    }
                }

                else if (eh_dados(&frame_resp))
                {   
                    if (frame_resp.sequencia == sequencia)
                    {
                        if (verifica_crc(&frame_resp))
                        {
                            print_frame(&frame_resp, arquivoTesteCliente);

                            fwrite(frame_resp.dados, sizeof(unsigned char), frame_resp.tamanho, arq);

                            // Manda ACK
                            if (!send_ack(sockfd, sequencia))
                                return EXIT_FAILURE;

                            sequencia = (sequencia + 1) % 32;
                        }
                        else
                        {
                            // Manda NACK
                            if (!send_nack(sockfd, sequencia))
                                return EXIT_FAILURE;
                        }
                    }
                    // Pode ocorrer de o server não receber o ACK pela mensagem anterior.
                    // Nesse sentido, ele ficaria reenviando até receber um ACK:
                    else if (sequencia == 0)
                    {
                        if (frame_resp.sequencia == 31)
                        {
                            // Manda ACK
                            if (!send_ack(sockfd, (unsigned char)31))
                                return EXIT_FAILURE;
                        }
                    }
                    else if (frame_resp.sequencia == sequencia - 1)
                    {
                        // Manda ACK
                        if (!send_ack(sockfd, sequencia - 1))
                            return EXIT_FAILURE;
                    }
                }

                if (recv(sockfd, &frame_resp, sizeof(frame_resp), 0) < 0)
                {
                    perror("Erro no recebimento:");
                    return EXIT_FAILURE;
                }
            }

            // Frame indicando final de transmissão recebido do server.

            fclose(arq);
            fclose(arquivoTesteCliente);

            if (!erroDisco && !outroErro)
            {
                if (!send_ack(sockfd, sequencia))
                    return EXIT_FAILURE;

                sequencia = (sequencia + 1) % 32;

                printf("\nDownload realizado com sucesso\n");
            }

            erroDisco = 0;
            outroErro = 0;
        }

        if (entradaOpcao == 2)
            break;
    }

    // Encerra o socket
    close(sockfd);
    fprintf(stdout, "\nConexão fechada\n\n");

    return EXIT_SUCCESS;
}
