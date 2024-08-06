#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>

#define MAX_PROCESSES 100
#define MAX_RESOURCES 100

int P; // Número de processos (threads)
int R; // Número de tipos de recursos

int available[MAX_RESOURCES]; // Recursos disponíveis
int max[MAX_PROCESSES][MAX_RESOURCES];
int allocation[MAX_PROCESSES][MAX_RESOURCES];
int need[MAX_PROCESSES][MAX_RESOURCES];

sem_t mutex; // Semáforo para controle de acesso

int gera_rand(int max, int min) {
    return rand() % (max - min + 1) + min;
}

int* gera_requisicao(int cliente) {
    int *req = malloc(R * sizeof(int)); // R é o número de tipos de recursos

    if (req == NULL) {
        fprintf(stderr, "Erro ao alocar memória para a requisição.\n");
        exit(1);
    }

    for (int i = 0; i < R; i++) {
        req[i] = gera_rand(need[cliente][i], 0); // Gera um número entre 0 e need[cliente][i]
    }

    return req;
}

bool isSafe() {
    int work[MAX_RESOURCES];
    bool finish[MAX_PROCESSES] = {0};

    for (int i = 0; i < R; i++)
        work[i] = available[i];

    int count = 0;
    while (count < P) {
        bool found = false;
        for (int p = 0; p < P; p++) {
            if (!finish[p]) {
                int j;
                for (j = 0; j < R; j++)
                    if (need[p][j] > work[j])
                        break;

                if (j == R) {
                    for (int k = 0; k < R; k++)
                        work[k] += allocation[p][k];

                    finish[p] = true;
                    found = true;
                    count++;
                }
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

bool requestResources(int process, int request[]) {
    sem_wait(&mutex);

    for (int i = 0; i < R; i++) {
        if (request[i] > need[process][i]) {
            printf("Erro: O processo %d solicitou mais recursos do que necessita.\n", process);
            sem_post(&mutex);
            return false;
        }
        if (request[i] > available[i]) {
            printf("Recursos indisponíveis para o processo %d.\n", process);
            sem_post(&mutex);
            return false;
        }
    }

    for (int i = 0; i < R; i++) {
        available[i] -= request[i];
        allocation[process][i] += request[i];
        need[process][i] -= request[i];
    }

    if (isSafe()) {
        printf("Recursos alocados ao processo %d.\n", process);
        sem_post(&mutex);
        return true;
    } else {
        for (int i = 0; i < R; i++) {
            available[i] += request[i];
            allocation[process][i] -= request[i];
            need[process][i] += request[i];
        }
        printf("O sistema não está em estado seguro após a solicitação do processo %d.\n", process);
        sem_post(&mutex);
        return false;
    }
}

void releaseResources(int process) {
    sem_wait(&mutex);
    for (int i = 0; i < R; i++) {
        available[i] += allocation[process][i];
        allocation[process][i] = 0;
        need[process][i] = max[process][i];
    }
    printf("Recursos liberados pelo processo %d.\n", process);
    sem_post(&mutex);
}

void* processCode(void* id) {
    int process = *((int*)id);

    // Gera uma requisição de recursos
    int *request = gera_requisicao(process);

    if (requestResources(process, request)) {
        // Simular uso de recursos
        sleep(1);
        releaseResources(process);
    }

    free(request); // Liberar a memória alocada para a requisição
    return NULL;
}

void imprime_matriz() {
    printf("\nRecursos Disponíveis:\n");
    for (int i = 0; i < R; i++) {
        printf("%d ", available[i]);
    }
    printf("\n");

    printf("\nMatriz Max:\n");
    for (int i = 0; i < P; i++) {
        for (int j = 0; j < R; j++) {
            printf("%d ", max[i][j]);
        }
        printf("\n");
    }

    printf("\nMatriz Allocation:\n");
    for (int i = 0; i < P; i++) {
        for (int j = 0; j < R; j++) {
            printf("%d ", allocation[i][j]);
        }
        printf("\n");
    }

    printf("\nMatriz Need:\n");
    for (int i = 0; i < P; i++) {
        for (int j = 0; j < R; j++) {
            printf("%d ", need[i][j]);
        }
        printf("\n");
    }
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Uso: %s <número de processos> <número de tipos de recursos>\n", argv[0]);
        return 1;
    }

    P = atoi(argv[1]);
    R = atoi(argv[2]);

    if (P > MAX_PROCESSES || R > MAX_RESOURCES) {
        fprintf(stderr, "Erro: número máximo de processos é %d e número máximo de recursos é %d.\n", MAX_PROCESSES, MAX_RESOURCES);
        return 1;
    }

    srand(time(NULL)); // Inicializa o gerador de números aleatórios

    // Inicializar recursos disponíveis
    printf("Gerando automaticamente a quantidade de cada recurso disponível:\n");
    for (int i = 0; i < R; i++) {
        available[i] = gera_rand(10, 1); // Gera um valor entre 1 e 10
        printf("Recurso %d: %d\n", i, available[i]);
    }

    // Inicializar matriz de máximo e de alocação
    printf("Gerando automaticamente a matriz de recursos máximos necessários (Max):\n");
    for (int i = 0; i < P; i++) {
        for (int j = 0; j < R; j++) {
            max[i][j] = gera_rand(10, 1); // Gera um valor entre 1 e 10
            allocation[i][j] = 0; // Inicialmente, nenhuma alocação
            need[i][j] = max[i][j]; // Need é inicialmente igual a Max
            printf("Max[%d][%d]: %d\n", i, j, max[i][j]);
        }
    }

    imprime_matriz(); // Chama a função para imprimir as matrizes

    pthread_t processes[P];
    int processIds[P];

    sem_init(&mutex, 0, 1);

    for (int i = 0; i < P; i++) {
        processIds[i] = i;
        pthread_create(&processes[i], NULL, processCode, (void*)&processIds[i]);
    }

    for (int i = 0; i < P; i++)
        pthread_join(processes[i], NULL);

    sem_destroy(&mutex);
    return 0;
}
