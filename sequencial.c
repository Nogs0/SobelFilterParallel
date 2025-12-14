#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

unsigned char *lerImagemPgm(char *path, int *width, int *height);
void salvarImagemPgm(const char *caminho, unsigned char *dados, int w, int h);
unsigned char getPixel(unsigned char *img, int w, int h, int x, int y);
void sobel(unsigned char *entrada, unsigned char *saida, int w, int h);

int main()
{
    int width, height;
    unsigned char *imagemEntrada = lerImagemPgm("sample.pgm", &width, &height);

    if (imagemEntrada == NULL)
    {
        printf("Erro ao ler imagem.");
        return 1;
    }

    unsigned char *imagemSaida = (unsigned char *)malloc(width * height);

    time_t inicio = clock();
    sobel(imagemEntrada, imagemSaida, width, height);
    time_t fim = clock();

    double tempoGasto = (double)(fim - inicio) / CLOCKS_PER_SEC;
    printf("Tempo gasto: %f segundos\n", tempoGasto);

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

void sobel(
    unsigned char *entradaLocal,
    unsigned char *saidaLocal,
    int largura,
    int altura)
{
    int Gx[3][3] = {{-1, 0, 1}, {-2, 0, 2}, {-1, 0, 1}};
    int Gy[3][3] = {{-1, -2, -1}, {0, 0, 0}, {1, 2, 1}};

    for (int y = 0; y < altura; y++)
    {
        for (int x = 0; x < largura; x++)
        {
            float sumX = 0, sumY = 0;

            for (int i = -1; i <= 1; i++)
            {
                for (int j = -1; j <= 1; j++)
                {
                    unsigned char val = getPixel(entradaLocal, largura, altura, x + j, y + i);
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
