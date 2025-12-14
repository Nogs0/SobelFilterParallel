#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include "mpi.h"

unsigned char *lerImagemPgm(char *path, int *width, int *height);
void salvarImagemPgm(const char *caminho, unsigned char *dados, int w, int h);
unsigned char getPixel(unsigned char *img, int w, int h, int x, int y);
void sobelParcial(unsigned char *entrada, unsigned char *saida, int w, int h);

int main(int argc, char *argv[])
{
    int width, height;
    int rank, size, i, tag = 100;
    unsigned char *imagemEntrada = NULL;
    unsigned char *imagemFinal = NULL;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (rank == 0) // processo mestre
    {
        imagemEntrada = lerImagemPgm("sample.pgm", &width, &height);
        printf("1. LEITURA DA IMAGEM REALIZADA - P0\n");
        if (imagemEntrada == NULL)
        {
            printf("Erro ao ler imagem.");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        imagemFinal = (unsigned char *)malloc(width * height);
        printf("2. MEMORIA PARA IMG FINAL ALOCADA - P0\n");
    }

    MPI_Bcast(&width, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&height, 1, MPI_INT, 0, MPI_COMM_WORLD);

    // MPI_Barrier(MPI_COMM_WORLD);
    double t1 = MPI_Wtime();

    printf("3. CALCULO DAS LINHAS QUE CADA PROCESSO VAI EXECUTAR P%d\n", rank);

    int minhasLinhas = height / size;
    int restoLinhas = height % size;

    if (rank == size - 1) // se for o ultimo fica com o resto
        minhasLinhas += restoLinhas;

    int *quantosBytesEuProcesso = (int *)malloc(size * sizeof(int));
    int *offsetParaInicio =  (int *)malloc(size * sizeof(int));

    int offsetInicial = 0;
    for (int i = 0; i < size; i++)
    {
        quantosBytesEuProcesso[i] = minhasLinhas * width;
        offsetParaInicio[i] = offsetInicial;
        offsetInicial += quantosBytesEuProcesso[i];
    }

    int temLinhaCima = (rank > 0); // se for o mestre n tem linha acima
    int temLinhaBaixo = (rank < size - 1); // se for o ultimo n tem linha abaixo

    int alturaLocal = minhasLinhas + temLinhaCima + temLinhaBaixo;

    unsigned char *meuPedacoDaImagemOriginal = (unsigned char *)malloc(alturaLocal * width);
    unsigned char *meuPedacoDaImagemProcessada = (unsigned char *)malloc(alturaLocal * width);

    if (rank == 0)
    {
        // copia a parte do mestre
        memcpy(meuPedacoDaImagemOriginal, imagemEntrada, alturaLocal * width);

        for (int i = 1; i < size; i++) //ignora o 0
        {
            int lEscravo = minhasLinhas;
            if (i == size - 1) // se for o ultimo soma o resto das linhas
            {
                lEscravo += restoLinhas;
            }

            int offsetComRecuo = offsetParaInicio[i] - width; // recua uma linha para pegar o vizinho de cima
            int qtdLinhasParaEnvio = lEscravo + 2; // soma dos vizinhos de cima e baixo

            if (i == size - 1) // remove vizinho de baixo do ultimo
                qtdLinhasParaEnvio--;

            printf("4. ENVIO DE DADOS PARA OS ESCRAVOS - P0 (MESTRE)\n");
            MPI_Send(imagemEntrada + offsetComRecuo, qtdLinhasParaEnvio * width, MPI_UNSIGNED_CHAR, i, 0, MPI_COMM_WORLD);
        }
    }
    else
    {
        printf("5. RECEBIMENTO DE DADOS VINDOS DO P0 (MESTRE) - P%d\n", rank);
        MPI_Recv(meuPedacoDaImagemOriginal, alturaLocal * width, MPI_UNSIGNED_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }

    printf("6. EXECUCAO DO SOBEL - P%d\n", rank);

    sobelParcial(meuPedacoDaImagemOriginal, meuPedacoDaImagemProcessada, width, alturaLocal);

    unsigned char *dadosDaFaixaValidos = meuPedacoDaImagemProcessada + (temLinhaCima * width);
    int qtdBytesValidos = minhasLinhas * width;

    if (rank > 0)
    {
        MPI_Send(dadosDaFaixaValidos, qtdBytesValidos, MPI_UNSIGNED_CHAR, 0, 1, MPI_COMM_WORLD);
    }
    else {
        memcpy(imagemFinal, dadosDaFaixaValidos, qtdBytesValidos);

        for (int i = 1; i < size; i++)
        {
            MPI_Recv(imagemFinal + offsetParaInicio[i], quantosBytesEuProcesso[i], MPI_UNSIGNED_CHAR, i, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }

        double t2 = MPI_Wtime();
        printf("Tempo final %f s\n", t2 - t1);
        printf("Processamento concluido.\n");
        salvarImagemPgm("saida_mpi.pgm", imagemFinal, width, height);
        free(imagemEntrada);
        free(imagemFinal);
    }


    // --- Limpeza ---
    free(meuPedacoDaImagemOriginal);
    free(meuPedacoDaImagemProcessada);
    free(quantosBytesEuProcesso);
    free(offsetParaInicio);

    MPI_Finalize();
    return 0;
}

unsigned char *lerImagemPgm(char *path, int *width, int *height)
{
    FILE *file = fopen(path, "rb");
    if (!file)
    {
        printf("Nao foi possivel encontrar o arquivo pelo caminho especificado");
        return NULL;
    }

    char buffer[16];
    int maxVal;
    fgets(buffer, sizeof(buffer), file);
    if (buffer[0] != 'P')
    {
        printf("Formato do arquivo deve ser .pgm");
        fclose(file);
        return NULL;
    }

    int c = getc(file);
    while (c == '#')
    {
        while (getc(file) != '\n')
            ;
        c = getc(file);
    }
    ungetc(c, file);

    if (fscanf(file, "%d %d", width, height) != 2)
    {
        printf("Erro ao ler dimensões da imagem");
        fclose(file);
        return NULL;
    }

    fscanf(file, "%d", &maxVal);
    fgetc(file);

    unsigned char *memoriaParaImg = (unsigned char *)malloc(*width * *height);
    if (memoriaParaImg == NULL)
    {
        printf("Erro ao alocar memória");
        fclose(file);
        return NULL;
    }

    if (fread(memoriaParaImg, 1, *width * *height, file) != (*width * *height))
    {
        printf("Erro ao ler imagem");
        free(memoriaParaImg);
        fclose(file);

        return NULL;
    }

    fclose(file);

    return memoriaParaImg;
}

void salvarImagemPgm(const char *caminho, unsigned char *dados, int w, int h)
{
    FILE *fp = fopen(caminho, "wb");
    if (!fp)
        return;

    fprintf(fp, "P5\n%d %d\n255\n", w, h);
    fwrite(dados, 1, w * h, fp);
    fclose(fp);
}

unsigned char getPixel(unsigned char *img, int w, int h, int x, int y)
{
    if (x < 0)
        x = 0;
    if (x >= w)
        x = w - 1;
    if (y < 0)
        y = 0;
    if (y >= h)
        y = h - 1;
    return img[y * w + x];
}

void sobelParcial(
    unsigned char *entradaLocal,
    unsigned char *saidaLocal,
    int largura,
    int alturaLocal)
{
    int Gx[3][3] = {{-1, 0, 1}, {-2, 0, 2}, {-1, 0, 1}};
    int Gy[3][3] = {{-1, -2, -1}, {0, 0, 0}, {1, 2, 1}};

    for (int y = 0; y < alturaLocal; y++)
    {
        for (int x = 0; x < largura; x++)
        {
            float sumX = 0, sumY = 0;

            for (int i = -1; i <= 1; i++)
            {
                for (int j = -1; j <= 1; j++)
                {
                    unsigned char val = getPixel(entradaLocal, largura, alturaLocal, x + j, y + i);
                    sumX += val * Gx[i + 1][j + 1];
                    sumY += val * Gy[i + 1][j + 1];
                }
            }
            int mag = (int)sqrt(sumX * sumX + sumY * sumY);
            if (mag > 255)
                mag = 255;
            if (mag < 0)
                mag = 0;

            saidaLocal[y * largura + x] = (unsigned char)mag;
        }
    }
}
