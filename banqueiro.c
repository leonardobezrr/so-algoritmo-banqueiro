#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>

// maximo limite individual de recursos
#define MAXLIMIT 15

// numero de clientes
int n;

// numero de recursos
int m;

// estruturas utilizadas pelo algoritmo

// recursos disponiveis
int * disp;
// maximo de recursos para cada processo
int ** max;
// quantidade de recursos alocadas por processo
int ** aloc;
// quantidade de recursos que cada processo ainda precisa
int ** ne;

// threads dos n clientes
pthread_t * ncli;
// semáforo global
sem_t mutex;

// funcoes utilizadas

// verifica se o estado eh seguro
int seguranca();

// verifica se uma requisicao eh valida
int requisicao(int, int *);

// a ser rodada pelas threads dos clientes
void cliente(void *);

// inicializa os valores disponiveis dos recursos
void inicia_recursos();

// inicializa os valores dos clientes:
//  - valores de max[][]
//  - valores de aloc[][]
//  - valores de ne[][]
void inicia_clientes();

// verifica se um cliente ja finalizou, se tem todos os recursos necessario
// se sim, libera seus recursos alocados
int finaliza_cliente(int);

// gera uma requisicao aleatoria
int * gera_requisicao();

// gera um valor aleatorio
// de 1 a lim, se zero = 1
// de 0 a lim, se zero = 0
int gera_rand(int lim, int zero);

// imprime o estado do banco
void print_banco();

int main(int argc, char ** argv)
{
    if ( argc != 3 )
    {
        printf("Uso: %s num_clientes num_recursos\n", argv[0]);
        return 1;
    }

    int i;
    
    // iniciando a semente do random
    srand(time(NULL));

    // num clients
    n = atoi(argv[1]);
    
    // num resources
    m = atoi(argv[2]);

    sem_init(&mutex, 0, 1);

    // iniciando os recursos
    inicia_recursos();
    
    // iniciando os clientes
    inicia_clientes();

    print_banco();
    
    // liberando a memoria alocada
    free(disp);
    free(max);
    free(aloc);
    free(ne);

    sem_destroy(&mutex); // destroi o semáforo no final
    
    return 0;
}

void inicia_recursos()
{
    int i;

    // gerando um vetor de recursos disponiveis
    disp = (int *)malloc(m * sizeof(int));

    for (i = 0; i < m; i++)
    {
        disp[i] = gera_rand(MAXLIMIT, 1);
    }
}

void inicia_clientes()
{
    int i, j;

    // criando as matrizes do banqueiro
    max  = (int **) malloc(n * sizeof(int *));
    aloc = (int **) malloc(n * sizeof(int *));
    ne   = (int **) malloc(n * sizeof(int *));
    
    // gerando uma lista de threads de clientes
    ncli = malloc(n * sizeof(pthread_t));

    // iniciando os valores maximos de cada cliente
    for (i = 0; i < n; i++)
    {
        max[i]  = (int *) malloc(m * sizeof(int));
        aloc[i] = (int *) malloc(m * sizeof(int));
        ne[i]   = (int *) malloc(m * sizeof(int));

        for (j = 0; j < m; j++)
        {
            max[i][j]  = gera_rand(disp[j], 1);
            aloc[i][j] = 0;
            ne[i][j]   = max[i][j];
        }
    
        // iniciando as threads dos clientes
        pthread_create(&ncli[i], NULL, (void *)cliente, (void *) (intptr_t) i);
    }
    
    // esperando as threads
    for (i = 0; i < n; i++)
    {
        pthread_join(ncli[i], NULL);
    }
    
}

int * gera_requisicao(int cliente)
{
    static int * req;
    req = malloc(m * sizeof(int));

    int i;
    for (i = 0; i < m; i++)
    {
        req[i] = gera_rand(ne[cliente][i], 0);
    }

    return req;
}

int requisicao(int i, int *req)
{
    int j;

    sem_wait(&mutex); // garante acesso exclusivo aos recursos

    // Verifica se a requisição é válida (req <= ne[i])
    for (j = 0; j < m; j++)
    {
        if (req[j] > ne[i][j] || req[j] > disp[j])
        {
            sem_post(&mutex); // libera o semáforo caso a requisição seja inválida
            return 0; // Requisição inválida
        }
    }

    // Simula a alocação
    for (j = 0; j < m; j++)
    {
        disp[j] -= req[j];
        aloc[i][j] += req[j];
        ne[i][j] -= req[j];
    }

    // Verifica se o sistema ainda está seguro
    if (seguranca())
    {
        sem_post(&mutex); // Libera o semáforo após uma requisição válida
        return 1; // Requisição aceita
    }
    else
    {
        // Rollback se o sistema não estiver seguro
        for (j = 0; j < m; j++)
        {
            disp[j] += req[j];
            aloc[i][j] -= req[j];
            ne[i][j] += req[j];
        }
        sem_post(&mutex); // Libera o semáforo após rollback
        return 0; // Requisição negada
    }
}



int seguranca()
{
    int i, j;

    int *trab = malloc(m * sizeof(int));
    int *fim = malloc(n * sizeof(int));

    // Inicializa o vetor de trabalho
    for (j = 0; j < m; j++)
    {
        trab[j] = disp[j];
    }

    // Inicializa o vetor de clientes terminados
    for (i = 0; i < n; i++)
    {
        fim[i] = 0;
    }

    int algum_terminou = 1;

    while (algum_terminou)
    {
        algum_terminou = 0;
        for (i = 0; i < n; i++)
        {
            if (!fim[i])
            {
                // Verifica se o cliente i pode terminar
                int pode_terminar = 1;
                for (j = 0; j < m; j++)
                {
                    if (ne[i][j] > trab[j])
                    {
                        pode_terminar = 0;
                        break;
                    }
                }
                if (pode_terminar)
                {
                    // O cliente pode terminar, libera seus recursos
                    for (j = 0; j < m; j++)
                    {
                        trab[j] += aloc[i][j];
                    }
                    fim[i] = 1;
                    algum_terminou = 1;
                }
            }
        }
    }

    // Verifica se todos os clientes terminaram
    for (i = 0; i < n; i++)
    {
        if (!fim[i])
        {
            free(trab);
            free(fim);
            return 0; // Estado inseguro
        }
    }

    free(trab);
    free(fim);
    return 1; // Estado seguro
}



int finaliza_cliente(int i)
{
    int j;

    // verifica se cliente nao tem mais necessidades
    for (j = 0; j < m; j++)
    {
        // cliente ainda nao terminou
        if ( ne[i][j] != 0 )
        {
            return 0;
        }
    }

    // cliente terminou

    // limpa seus recursos
    for (j = 0; j < m; j++)
    {
        // devolve para os disponiveis
        disp[j] += aloc[i][j];
        aloc[i][j] = 0;
    }

    return 1;
}

void cliente(void *param_i)
{
    int i = (intptr_t) param_i;

    int executa = 1, j;

    int * req;
    
    // fica em looping, gerando requisições, até conseguir todos os recursos
    // que precisa, quando conseguir todos os recursos, irá finalizar
    while(executa)
    {
        
        printf(": Cliente %d gerou a requisição: ",i);

        req = gera_requisicao(i);

        for (j = 0; j < m; j++)
        {
            printf(" %2d ",req[j]);
        }

        printf("\n");
        
        printf(": Cliente %d ganhou acesso\n",i);

        // enviando a requisição ao banco
        if ( requisicao(i, req) == 1 )
        {
            printf(": Cliente %d executou sua requisicao\n",i);

            // verifica se o cliente terminou
            if ( finaliza_cliente(i) == 1 )
            {
            
                printf(": Cliente %d finalizou\n",i);

                // cliente ja terminou, seus recursos foram liberados
                executa = 0;
            }
        }
        else
        {
            printf(": Cliente %d teve sua requisicao negada\n",i);
        }

        print_banco();
    }
}

int gera_rand(int limit, int zero)
{
    if (zero == 1)
    {
        // 1 a limit
        return (rand() % limit) + 1;
    }
    
    // 0 a limit
    return rand() % (limit + 1);
    
}

void print_banco()
{
    int i, j;

    printf("\t== BANCO ==\n");

    printf("Recursos disponiveis\n");
    printf("\t    i: ");
    for (j = 0; j < m; j++)
    {
        printf(" %2d ",j);
    }
    
    printf("\n\t       ");
    for (j = 0; j < m; j++)
    {
        printf(" %2d ",disp[j]);
    }

    printf("\n");

    printf("MAX dos clientes\n");
    for (i = 0; i < n; i++)
    {
        printf("\ti: %d [ ",i);

        for (j = 0; j < m; j++)
        {
            printf(" %2d ",max[i][j]);
        }
    
        printf(" ]\n");
    }
    
    printf("Aloc dos clientes\n");
    for (i = 0; i < n; i++)
    {
        printf("\ti: %d [ ",i);

        for (j = 0; j < m; j++)
        {
            printf(" %2d ",aloc[i][j]);
        }
    
        printf(" ]\n");
    }
    
    printf("Ne dos clientes\n");
    for (i = 0; i < n; i++)
    {
        printf("\ti: %d [ ",i);

        for (j = 0; j < m; j++)
        {
            printf(" %2d ",ne[i][j]);
        }
    
        printf(" ]\n");
    }
    
    printf("========================\n");
    printf("\n");
}
