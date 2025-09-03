#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <sys/time.h>
#include <pthread.h>
#include <semaphore.h>

int **matriz_NM;
int **matriz_MN;
int **matriz_R;
int N;
int M;
typedef struct { int i, j; } ij;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  conds = PTHREAD_COND_INITIALIZER;
sem_t semaforo;
sem_t celss;
#define MAX_CONCURRENCY 8


void* calc_cell(void* arg){
    ij* point = (ij*)arg;
    int i = point->i;
    int j = point->j;
    int cell = 0;
    for (int k = 0;k < N; ++k){
        cell = cell + matriz_MN[i][k] * matriz_NM[k][j];
    }
    matriz_R[i][j] = cell;
    free(point);
    pthread_mutex_lock(&mutex);
    semaforo++;                       
    pthread_cond_signal(&conds);
    pthread_mutex_unlock(&mutex);
    return NULL; 
}

int** mult_matrices(int **matriz_NM, int **matriz_MN){
    int **matriz_R = (int**)malloc(M * sizeof(int*)); 
    pthread_t *threads = malloc(M * M * sizeof(pthread_t));
    int pid = 0;
    for(int i = 0; i < M; i++){
        matriz_R[i] = (int*)calloc(M, sizeof(int));
        for(int j=0;j < M; j++){
            ij *arg = malloc(sizeof(ij));
            arg->i = i; 
            arg->j = j;
            pthread_mutex_lock(&mutex);
            while (semaforo == 0) {
                // espera a que alguien libere cupo; wait suelta el mutex y lo retoma al despertar
                pthread_cond_wait(&conds, &mutex);   // ver doc IBM: mutex debe estar lockeado
            }
            semaforo--;
            pthread_mutex_unlock(&mutex);
            pthread_create(&threads[pid++], NULL, calc_cell, arg);
            //printf("%d ,", matriz_R[i][j]);
        }
        //printf("\n");
    }
    for (int t = 0; t < pid; ++t){
        pthread_join(threads[t], NULL);
    }
    free(threads);
    return matriz_R;
}

int** matrices(int N, int M){
    int **matriz = (int**)malloc(N * sizeof(int*));
    srand(time(NULL));
    //printf("Matriz NxM = \n");
    for(int i = 0; i < N; ++i){
        matriz[i] = (int*)malloc(M * sizeof(int));
        for(int j = 0; j < M; j++){
            matriz[i][j] = 1 + rand() % 5;
            //printf("%d ,", matriz[i][j]);
        }
        //printf("\n");
    }
    return matriz;
}

int main (){
    printf("\nN = ");
    scanf("%d", &N);
    printf("\nM = ");
    scanf("%d", &M);

    semaforo = MAX_CONCURRENCY;

    struct timeval start, end;
    long times[100];
    for(int rep = 0; rep < 100; rep++){
        //printf("Matriz NxM: \n");
        matriz_NM = matrices(N, M);
        //printf("Matriz MxN: \n");
        matriz_MN = matrices(M, N);
        //printf("Matriz Result: \n");
        gettimeofday(&start, NULL);
        matriz_R = mult_matrices(matriz_NM, matriz_MN);
        gettimeofday(&end, NULL);

        long duration = (end.tv_sec - start.tv_sec)*1000000L + (end.tv_usec - start.tv_usec);
        times[rep] = duration;
    }
    double avg;
    double sum = 0;
    for(int i = 0; i < 100; i++){
        sum = sum + times[i];
    }
    avg = sum / 100;
    sum = 0;
    for(int i = 0; i < 100; i++){
        double dif = times[i] - avg;
        sum = sum + dif * dif;
    }
    double dev = sqrt(sum / 100);
    printf("AVG = %.2f \n",avg);
    printf("DEV = %.2f \n", dev); 
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&conds);
    
}