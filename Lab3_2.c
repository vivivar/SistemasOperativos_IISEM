#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <sys/time.h>
#include <pthread.h>

#define MAX_CONCURRENCY 8

typedef struct {
    int row, col;         
    int N;                
    int **matriz_MN;              
    int **matriz_NM_trans;             
    int **matriz_R;              
} Task;

static pthread_mutex_t mutexSpace = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  condsSpace  = PTHREAD_COND_INITIALIZER;
static int space = MAX_CONCURRENCY;

static pthread_mutex_t readyMutex  = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  readyConds   = PTHREAD_COND_INITIALIZER;
static long ready = 0;

static int** matrices(int rows, int cols){
    int **m = (int**)malloc(rows * sizeof(int*));
    if(!m){ perror("malloc rows"); exit(1); }
    for(int i=0;i<rows;i++){
        m[i] = (int*)malloc(cols * sizeof(int));  
        if(!m[i]){ perror("malloc cols"); exit(1); }
    }
    return m;
}

static void liberar_matriz(int **m, int rows){
    if(!m) return;
    for(int i=0;i<rows;i++) free(m[i]);
    free(m);
}

static void llenar_matriz(int **m, int rows, int cols){
    for(int i=0;i<rows;i++)
        for(int j=0;j<cols;j++)
            m[i][j] = 1 + rand()%5;  // [1..5]
}

static int** transpose_A(int **matriz_NM, int N, int M){
    int **matriz_NM_trans = matrices(M, N);
    for(int j=0;j<M;j++)
        for(int k=0;k<N;k++)
            matriz_NM_trans[j][k] = matriz_NM[k][j];
    return matriz_NM_trans;
}

static void* calc_cell(void *arg){
    Task *t = (Task*)arg;
    const int i = t->row, j = t->col, N = t->N;
    int acc = 0;

    for(int k=0;k<N;k++)
        acc += t->matriz_MN[i][k] * t->matriz_NM_trans[j][k];
    t->matriz_R[i][j] = acc;

    free(t);

    pthread_mutex_lock(&readyMutex);
    ready++;
    pthread_cond_signal(&readyConds);
    pthread_mutex_unlock(&readyMutex);

    pthread_mutex_lock(&mutexSpace);
    space++;
    pthread_cond_signal(&condsSpace);
    pthread_mutex_unlock(&mutexSpace);

    return NULL;
}

static int** mult_matrices_threads(int **matriz_NM, int **matriz_MN, int N, int M){
    int **matriz_R  = matrices(M, M);
    int **matriz_NM_trans = transpose_A(matriz_NM, N, M);

    pthread_mutex_lock(&mutexSpace);
    space = MAX_CONCURRENCY;
    pthread_mutex_unlock(&mutexSpace);

    pthread_mutex_lock(&readyMutex);
    ready = 0;
    pthread_mutex_unlock(&readyMutex);

    for(int i=0;i<M;i++){
        for(int j=0;j<M;j++){
            pthread_mutex_lock(&mutexSpace);
            while (space == 0) {
                pthread_cond_wait(&condsSpace, &mutexSpace);
            }
            space--;
            pthread_mutex_unlock(&mutexSpace);

            Task *t = (Task*)malloc(sizeof(Task));
            t->row = i; t->col = j;
            t->N = N; t->matriz_MN = matriz_MN; t->matriz_NM_trans = matriz_NM_trans; t->matriz_R = matriz_R;

            pthread_t th;
            int rc = pthread_create(&th, NULL, calc_cell, t);
            if (rc != 0) { perror("pthread_create"); exit(1); }
            pthread_detach(th);
        }
    }

    pthread_mutex_lock(&readyMutex);
    while (ready < (long)M * (long)M) {
        pthread_cond_wait(&readyConds, &readyMutex);
    }
    pthread_mutex_unlock(&readyMutex);
    liberar_matriz(matriz_NM_trans, M);
    return matriz_R;
}

int main (void){
    int N_; 
    int M_;
    printf("\nN = ");
    scanf("%d", &N_);
    printf("\nM = ");
    scanf("%d", &M_);
    int N = N_;
    int M = M_;
    srand((unsigned)time(NULL));
    struct timeval start, end;
    const int repeticiones = 100;
    long times[repeticiones];

    for(int rep = 0; rep < repeticiones; rep++){
        int **matriz_NM = matrices(N, M);
        int **matriz_MN = matrices(M, N);
        llenar_matriz(matriz_NM, N, M);
        llenar_matriz(matriz_MN, M, N);

        gettimeofday(&start, NULL);
        int **matriz_R = mult_matrices_threads(matriz_NM, matriz_MN, N, M);   // matriz_R: M x M
        gettimeofday(&end, NULL);

        long duration = (end.tv_sec - start.tv_sec)*1000000L
                      + (end.tv_usec - start.tv_usec);
        times[rep] = duration;
        liberar_matriz(matriz_NM, N);
        liberar_matriz(matriz_MN, M);
        liberar_matriz(matriz_R, M);
    }

    //Promedio
    double sum = 0.0;
    for(int i=0;i<repeticiones;i++) {
        sum += times[i];
    }
    double avg = sum / (double)repeticiones;

    //Desviación
    double var = 0.0;
    for(int i=0;i<repeticiones;i++){
        double d = (double)times[i] - avg;
        var += d * d;
    }
    double dev = sqrt(var / (double)repeticiones);

    printf("AVG = %.2f us\n", avg);
    printf("DEV = %.2f us\n", dev);

    pthread_mutex_destroy(&mutexSpace);
    pthread_cond_destroy(&condsSpace);
    pthread_mutex_destroy(&readyMutex);
    pthread_cond_destroy(&readyConds);

    return 0;
}
