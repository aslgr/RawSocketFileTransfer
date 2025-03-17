#include "libServer.h"

int main(int argc, char *argv[]) 
{
    int sockfd, bytesEscritos;
    unsigned char dados[TAM_DADOS], sequencia = 0;
    char videoBuffer[TAM_DADOS], caminhoCompleto[TAM_DADOS + 9];
    int retornoWaitACK = 0;
    int deuErro = 0, codErro;

    FILE *arq, *arquivoTesteServer, *arquivoLocal;

    frame_t frame;
    frame_t frame_resp;
    inicializa_frame(&frame);
    inicializa_frame(&frame_resp);

    fprintf(stdout, "Iniciando Server ....\n");

    // Cria o Raw Socket
    sockfd = cria_raw_socket("lo");
    if (sockfd == -1) 
    {
        perror("Error on server socket creation:");
        return EXIT_FAILURE;
    }
    fprintf(stdout, "Socket do servidor criado com descritor: %d\n", sockfd);
    fprintf(stdout, "-------------------------------------------------------------------\n\n");

    // Começa a receber os dados
    if (recv(sockfd, &frame, sizeof(frame), 0) < 0)
    {
        perror("Erro no recebimento:");
        return EXIT_FAILURE;
    }

    // LOOP PRINCIPAL DO SERVER
    while (1)
    {
        // recebeu um "LISTA"
        if (eh_lista(&frame))
        {
            fprintf(stdout, "Comando LISTA recebido (automático)\n");

            // Manda ACK
            if (!send_ack(sockfd, sequencia))
                return EXIT_FAILURE;
            fprintf(stdout, "--ACK enviado\n");

            sequencia = (sequencia + 1) % 32;
            
            // Vasculha na pasta dos vídeos pelos nomes
            struct dirent *entry;

            DIR *dir = opendir(CAMINHO_VIDEOS);
            if (dir == NULL)
            {
                perror("opendir");
                return 1;
            }

            while ((entry = readdir(dir)))
            {
                if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, ".."))
                {
                    // Prepara e envia o frame com o nome do vídeo
                    strncpy((char*)frame_resp.dados, entry->d_name, TAM_DADOS-1);

                    // Monta sem usar a função pra não dar problema com string
                    frame_resp.marcadorInicio = 0x7E;
                    frame_resp.tamanho = (unsigned char)strlen(entry->d_name);
                    frame_resp.sequencia = sequencia;
                    frame_resp.tipo = 0x12;
                    frame_resp.crc8 = calcula_crc(&frame_resp);

                    sequencia = (sequencia + 1) % 32;

                    //printf("%s\n", frame_resp.dados);

                    if (send(sockfd, &frame_resp, sizeof(frame_resp), 0) < 0)
                    {
                        perror("Erro no envio:");
                        return EXIT_FAILURE;
                    }

                    if (wait_ack(sockfd, &frame_resp, 1000) == 0)
                        return EXIT_FAILURE;
                }
            }

            closedir(dir);

            // Prepara FIM_TX
            memset(dados, 0, TAM_DADOS);
            frame_resp = monta_mensagem(0x00, sequencia, 0x1E, dados, 0);
            sequencia = (sequencia + 1) % 32;

            // Envia FIM_TX
            if (send(sockfd, &frame_resp, sizeof(frame_resp), 0) < 0)
            {
                perror("Erro no envio:");
                return EXIT_FAILURE;
            }

            if (wait_ack(sockfd, &frame_resp, 1000) == 0)
                return EXIT_FAILURE;

            fprintf(stdout, "Todos os nomes dos videos foram enviados\n\n");
            fprintf(stdout, "-------------------------------------------------------------------\n\n");
        }
        
        // recebeu um "BAIXAR"
        else if (eh_baixar(&frame))
        {
            arquivoTesteServer = fopen("ArquivoTesteServer", "wb");
            if (!arquivoTesteServer)
            {
                perror("Erro ao abrir/criar o arquivo");
                return EXIT_FAILURE;
            }

            arquivoLocal = fopen("ArquivoLocal", "wb");
            if (!arquivoLocal)
            {
                perror("Erro ao abrir/criar o arquivo");
                return EXIT_FAILURE;
            }

            fprintf(stdout, "Comando BAIXAR recebido (usuário)\n");

            if (verifica_crc(&frame))
            {
                // Manda ACK
                if (!send_ack(sockfd, sequencia))
                    return EXIT_FAILURE;
                fprintf(stdout, "--ACK enviado\n");

                sequencia = (sequencia + 1) % 32;

                strncpy(videoBuffer, (char*)frame.dados, TAM_DADOS-1);
                snprintf(caminhoCompleto, TAM_DADOS + 9, "%s%s", CAMINHO_VIDEOS, videoBuffer);

                printf("Baixando arquivo: %s\n\n", caminhoCompleto);

                arq = fopen(caminhoCompleto, "rb");
                if (!arq) 
                {
                    //perror("Erro ao abrir o arquivo");
                    deuErro = 1;

                    // Verifica por que o arquivo não abriu
                    if (errno == EACCES)
                    {
                        fprintf(stdout, "Impossível baixar o arquivo: Permissões insuficientes.\n");
                        codErro = ERRO_ACESSO_NEGADO;
                    }
                    else if (errno == ENOENT)
                    {
                        fprintf(stdout, "Impossível baixar o arquivo: Arquivo não encontrado.\n");
                        codErro = ERRO_NAO_ENCONTRADO;
                    }
                    else
                    {
                        fprintf(stdout, "Impossível baixar o arquivo: %s\n", strerror(errno));
                        return EXIT_FAILURE;
                    }
                }

                if (deuErro)
                {
                    unsigned char bytes[4];
                    int_para_unsigned_char(codErro, bytes, sizeof(int));

                    // Prepara frame de Erro
                    frame_resp = monta_mensagem(sizeof(bytes), sequencia, 0x1F, bytes, 1);
                    sequencia = (sequencia + 1) % 32;
                    print_frame(&frame_resp, stdout);

                    // Envia frame de ERRO
                    if (send(sockfd, &frame_resp, sizeof(frame_resp), 0) < 0)
                    {
                        perror("Erro no envio:");
                        return EXIT_FAILURE;
                    }
                    printf("Frame com erro enviado para o client\n");

                    // Espera pelo ACK
                    if (wait_ack(sockfd, &frame_resp, 1000) == 0)
                            return EXIT_FAILURE;
                    printf("--ACK recebido\n");
                    fprintf(stdout, "-------------------------------------------------------------------\n\n");
                }

                if (!deuErro)
                {
                    // Envia 'descritor_arquivo' 

                    // Pega tamanho do vídeo
                    struct stat st;
                    stat(caminhoCompleto, &st);
                    int64_t tamVideo = st.st_size;

                    // Prepara frame com tamanho do video
                    unsigned char bytes[8];
                    int64_para_bytes(tamVideo, bytes);

                    frame_resp = monta_mensagem(sizeof(bytes), sequencia, 0x11, bytes, 1);
                    sequencia = (sequencia + 1) % 32;
                    print_frame(&frame_resp, arquivoTesteServer);

                    // Envia frame com tamanho do video
                    if (send(sockfd, &frame_resp, sizeof(frame_resp), 0) < 0)
                    {
                        perror("Erro no envio:");
                        return EXIT_FAILURE;
                    }
                    fprintf(stdout, "Frame DESCRITOR ARQUIVO (Tamanho do video) enviado\n");

                    // Espera pelo ACK
                    if (wait_ack(sockfd, &frame_resp, 1000) == 0)
                        return EXIT_FAILURE;
                    fprintf(stdout, "--ACK Recebido\n");

                    // Pega data da última modificação
                    char versaoVideo[32];
                    strftime(versaoVideo, sizeof(versaoVideo), "%Y-%m-%d %H:%M:%S", localtime(&st.st_mtime));

                    // Prepara frame com data da ultima modificação
                    strncpy((char*)frame_resp.dados, versaoVideo, TAM_DADOS-1);

                    // Monta sem usar a função pra não dar problema com string
                    frame_resp.marcadorInicio = 0x7E;
                    frame_resp.tamanho = (unsigned char)strlen(versaoVideo);
                    frame_resp.sequencia = sequencia;
                    frame_resp.tipo = 0x11;
                    frame_resp.crc8 = calcula_crc(&frame_resp);

                    sequencia = (sequencia + 1) % 32;
                    print_frame(&frame_resp, arquivoTesteServer);

                    // Envia frame com tamanho do video
                    if (send(sockfd, &frame_resp, sizeof(frame_resp), 0) < 0)
                    {
                        perror("Erro no envio:");
                        return EXIT_FAILURE;
                    }
                    fprintf(stdout, "Frame DESCRITOR ARQUIVO (Data da última modificação) enviado\n");

                    // Espera pelo ACK
                    if (wait_ack(sockfd, &frame_resp, 1000) == 0)
                        return EXIT_FAILURE;
                    fprintf(stdout, "--ACK Recebido\n");

                    fprintf(stdout, "Transferindo video...\n");

                    while((bytesEscritos = fread(dados, sizeof(unsigned char), TAM_DADOS-1, arq)) > 0)
                    {
                        fwrite(dados, sizeof(unsigned char), (unsigned char)bytesEscritos, arquivoLocal);

                        // Prepara e envia o frame com os dados do vídeo
                        frame_resp = monta_mensagem((unsigned char)bytesEscritos, sequencia, 0x12, dados, 1);
                        sequencia = (sequencia + 1) % 32;

                        print_frame(&frame_resp, arquivoTesteServer);

                        if (send(sockfd, &frame_resp, sizeof(frame_resp), 0) < 0)
                        {
                            perror("Erro no envio:");
                            return EXIT_FAILURE;
                        }

                        retornoWaitACK = wait_ack(sockfd, &frame_resp, 1000);

                        if (retornoWaitACK == 0)
                            return EXIT_FAILURE;
                        else if (retornoWaitACK == 2)
                            break;
                    }

                    fclose(arq);
                    fclose(arquivoTesteServer);
                    fclose(arquivoLocal);

                    if (retornoWaitACK == 2)
                    {
                        while (!eh_erro(&frame))
                        {
                            if (recv(sockfd, &frame, sizeof(frame), 0) < 0)
                            {
                                perror("Erro no recebimento:");
                                return 0;
                            }
                        }

                        // Manda ACK
                        if (!send_ack(sockfd, sequencia))
                            return EXIT_FAILURE;
                        sequencia = (sequencia + 1) % 32;

                        printf("ERRO %d DETECTADO NO CLIENT\n", bytes_para_int(frame.dados, frame.tamanho));

                        retornoWaitACK = 0;
                    }
                    else
                    {
                        // Prepara FIM_TX
                        memset(dados, 0, TAM_DADOS);
                        frame_resp = monta_mensagem(0x00, sequencia, 0x1E, dados, 0);
                        sequencia = (sequencia + 1) % 32;

                        // Envia FIM_TX
                        if (send(sockfd, &frame_resp, sizeof(frame_resp), 0) < 0)
                        {
                            perror("Erro no envio:");
                            return EXIT_FAILURE;
                        }

                        if (wait_ack(sockfd, &frame_resp, 1000) == 0)
                            return EXIT_FAILURE;
                    }

                    fprintf(stdout, "Transferência do vídeo para o client concluída\n");
                    fprintf(stdout, "-------------------------------------------------------------------\n\n");
                }
                deuErro = 0;
            }
            else
            {
                if (!send_nack(sockfd, sequencia))
                    return EXIT_FAILURE;
            }
        }

        if (recv(sockfd, &frame, sizeof(frame), 0) < 0)
        {
            perror("Erro no recebimento:");
            return EXIT_FAILURE;
        }
    }

    // Close socket
    close(sockfd);

    printf("Conexão fechada\n\n");

    return EXIT_SUCCESS;
}
