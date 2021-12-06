#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <pthread.h>

// 宏定义, 不管是任务队列节点还是工作队列节点都能使用
#define LL_ADD(item, list) do {             \
    item->prev = NULL;                      \
    item->next = list;                      \
    if (list != NULL) list->prev = item;    \
    list = item;                            \
} while(0)

#define LL_REMOVE(item, list) do {                              \
    if (item->prev != NULL) item->prev->next = item->next;      \
    if (item->next != NULL) item->next->prev = item->prev;      \
    if (list == item) list = item->next;                        \
    item->prev = item->next = NULL;                             \
} while(0)

typedef struct NWORKER {

    pthread_t   id;
    int terminate; // 终止任务的指示

    struct NTHREADPOOL *pool;

    struct NWORKER *prev;
    struct NWORKER *next;

} nworker;

typedef struct NJOB {
    
    void (*job_func)(struct NJOB *job);
    void *user_data;

    struct NJOB *prev;
    struct NJOB *next;

} njob;

typedef struct NTHREADPOOL {

    struct NWORKER *workers;
    struct NJOB *wait_jobs;

    pthread_cond_t cond;
    pthread_mutex_t mtx;

} nthreadpool;

void *thread_callback(void *arg) {
    
    nworker *worker = (nworker *) arg;

    while (1) {

        pthread_mutex_lock(&worker->pool->mtx);
        while (worker->pool->wait_jobs == NULL) {
            if (worker->terminate) break;
            pthread_cond_wait(&worker->pool->cond, &worker->pool->mtx);
        }

        if (worker->terminate) {
            pthread_mutex_unlock(&worker->pool->mtx);
            break;
        }

        njob *job = worker->pool->wait_jobs;
        if (job) {
            LL_REMOVE(job, worker->pool->wait_jobs);
        }

        pthread_mutex_unlock(&worker->pool->mtx);

        if (job == NULL) continue;

        job->job_func(job);

    }

    free(worker);

    return 0;
}

int thread_pool_create(nthreadpool *pool, int num_thread) {

    if (pool == NULL) return -1;

    if (num_thread < 1) num_thread = 1;
    memset(pool, 0, sizeof(nthreadpool));

    pthread_cond_t blank_cond = PTHREAD_COND_INITIALIZER;
    memcpy(&pool->cond, &blank_cond, sizeof(pthread_cond_t));

    pthread_mutex_t blank_mutex = PTHREAD_MUTEX_INITIALIZER;
    memcpy(&pool->mtx, &blank_mutex, sizeof(pthread_mutex_t));

    // (thread, worker)
    int idx = 0;
    for (idx = 0 ; idx < num_thread ; idx ++) {
        nworker *worker = (nworker *) malloc(sizeof(nworker));
        if (worker == NULL) {
            perror("malloc");
            return idx;
        }
        memset(worker, 0, sizeof(nworker));

        worker->pool = pool;

        int ret = pthread_create(&worker->id, NULL, thread_callback, worker);
        // (线程ID, 线程属性, 线程入口函数, 线程结构体)

        if (ret) {
            perror("pthread_create");
            free(worker);
            return idx;
        }
        LL_ADD(worker, pool->workers);
    }

    return idx;
}

int thread_pool_destroy(nthreadpool *pool) {

    nworker *worker = NULL;
    for (worker = pool->workers ; worker != NULL ; worker = worker->next) {
        worker->terminate = 1;
    }

    pthread_mutex_lock(&pool->mtx);
    pthread_cond_broadcast(&pool->cond);
    pthread_mutex_unlock(&pool->mtx);

    return 0;

}

int thread_pool_push_job(nthreadpool *pool, njob *job) {

    pthread_mutex_lock(&pool->mtx);
    LL_ADD(job, pool->wait_jobs);
    pthread_cond_signal(&pool->cond);
    pthread_mutex_unlock(&pool->mtx);

    return 0;
}

#if 1   // debug

#define TASK_COUNT          1000

void counter(njob *job) {

    if (job == NULL) return ;

    int idx = *(int *) job->user_data;

    printf("idx : %d, selfid : %lu\n", idx, pthread_self());

    free(job->user_data);
    free(job);

}

/*  gcc Threadpool.c -lpthread -o main && ./main  */
int main() {

    nthreadpool pool = {0};
    int num_thread = 50;

    thread_pool_create(&pool, num_thread);

    int i = 0;
    for (i = 0 ; i < TASK_COUNT ; i ++) {

        njob *job = (njob *) malloc(sizeof(njob));
        if (job == NULL) exit(1);

        job->job_func = counter;
        job->user_data = malloc(sizeof(int));
        *(int *) job->user_data = i;

        thread_pool_push_job(&pool, job);

    }

    getchar();

    return 0;
}

#endif