#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <semaphore.h>


#define REPETICIONES_DEF 100     
#define POOL_DEF         8     

static int** alloc_matrix(int rows, int cols){
    int **m = (int**)malloc(rows * sizeof(int*));
    if(!m){ perror("malloc rows"); exit(1); }
    for(int i=0;i<rows;i++){
        m[i] = (int*)malloc(cols * sizeof(int));
        if(!m[i]){ perror("malloc cols"); exit(1); }
    }
    return m;
}

static void free_matrix(int **m, int rows){
    if(!m) return;
    for(int i=0;i<rows;i++) free(m[i]);
    free(m);
}

static void fill_random_1_5(int **m, int rows, int cols){
    for(int i=0;i<rows;i++)
        for(int j=0;j<cols;j++)
            m[i][j] = 1 + rand()%5;
}

static int** transpose_A(int **A, int N, int M){
    int **AT = alloc_matrix(M, N);
    for(int j=0;j<M;j++)
        for(int k=0;k<N;k++)
            AT[j][k] = A[k][j];
    return AT;
}


typedef struct {
    sem_t  job_lock;     
    size_t next_job;     
} WorkQueue;


static int** mult_matrices_fork_pool(int **A, int **B, int N, int M, int POOL_PROCS){
    int **AT = transpose_A(A, N, M);
    size_t cells  = (size_t)M * (size_t)M;
    size_t bytesR = cells * sizeof(int);
    int *R_sh = mmap(NULL, bytesR, PROT_READ|PROT_WRITE,
                     MAP_SHARED|MAP_ANONYMOUS, -1, 0);
    if(R_sh == MAP_FAILED){ perror("mmap R"); exit(1); }

    WorkQueue *q = mmap(NULL, sizeof(WorkQueue), PROT_READ|PROT_WRITE,
                        MAP_SHARED|MAP_ANONYMOUS, -1, 0);
    if(q == MAP_FAILED){ perror("mmap WorkQueue"); exit(1); }
    if(sem_init(&q->job_lock, 1, 1) == -1){ perror("sem_init job_lock"); exit(1); }
    q->next_job = 0;

    int vivos = 0;
    for(int p=0; p<POOL_PROCS; ++p){
        pid_t pid = fork();
        if(pid == 0){
            for(;;){
                if(sem_wait(&q->job_lock) == -1){ perror("sem_wait"); _exit(1); }
                size_t idx = q->next_job++;
                if(sem_post(&q->job_lock) == -1){ perror("sem_post"); _exit(1); }

                if(idx >= cells) break; 

                int i = (int)(idx / (size_t)M);
                int j = (int)(idx % (size_t)M);

                long suma = 0;
                for(int k=0;k<N;k++){
                    suma += (long)B[i][k] * (long)AT[j][k];
                }
                R_sh[idx] = (int)suma;
            }
            _exit(0);
        } else if(pid > 0){
            vivos++;
        } else {
            perror("fork");
        }
    }

    while(vivos > 0){
        if(wait(NULL) > 0) vivos--;
    }

    sem_destroy(&q->job_lock);
    munmap(q, sizeof(WorkQueue));
    int **R = alloc_matrix(M, M);
    for(int i=0;i<M;i++)
        for(int j=0;j<M;j++)
            R[i][j] = R_sh[(size_t)i*(size_t)M + (size_t)j];

    munmap(R_sh, bytesR);
    free_matrix(AT, M);

    return R;
}


int main(void){
    int N, M;
    printf("\nN = ");
    if (scanf("%d", &N) != 1){ fprintf(stderr, "Entrada inválida para N\n"); return 1; }
    printf("\nM = ");
    if (scanf("%d", &M) != 1){ fprintf(stderr, "Entrada inválida para M\n"); return 1; }

    long cores = sysconf(_SC_NPROCESSORS_ONLN);
    int POOL_PROCS = (cores > 0) ? (int)cores : POOL_DEF;

    int REPETICIONES = REPETICIONES_DEF;
    srand((unsigned)time(NULL));

    struct timeval start, end;
    long *times = (long*)malloc(REPETICIONES * sizeof(long));
    if(!times){ perror("malloc times"); return 1; }

    for(int rep=0; rep<REPETICIONES; ++rep){
        int **matriz_NM = alloc_matrix(N, M);
        int **matriz_MN = alloc_matrix(M, N);
        fill_random_1_5(matriz_NM, N, M);
        fill_random_1_5(matriz_MN, M, N);

        gettimeofday(&start, NULL);
        int **matriz_R = mult_matrices_fork_pool(matriz_NM, matriz_MN, N, M, POOL_PROCS);
        gettimeofday(&end, NULL);

        long duration = (end.tv_sec - start.tv_sec)*1000000L
                      + (end.tv_usec - start.tv_usec);
        times[rep] = duration;

        free_matrix(matriz_NM, N);
        free_matrix(matriz_MN, M);
        free_matrix(matriz_R, M);
    }

    double sum = 0.0;
    for(int i=0;i<REPETICIONES;i++) {
        sum += times[i];
    }
    double avg = sum / (double)REPETICIONES;

    double var = 0.0;
    for(int i=0;i<REPETICIONES;i++){
        double d = (double)times[i] - avg;
        var += d * d;
    }
    double dev = sqrt(var / (double)REPETICIONES);
    printf("AVG = %.2f us\n", avg);
    printf("DEV = %.2f us\n", dev);

    free(times);
    return 0;
}
