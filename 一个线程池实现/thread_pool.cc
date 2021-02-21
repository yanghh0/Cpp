#include <pthread.h>
#include <stdio.h>
#include<string.h>
#include <stdlib.h>

// Head insert
#define LL_ADD(node, head) do { \
    node->prev = NULL;          \
    node->next = head;          \
    if(head != NULL)            \
        head->prev = node;      \
    head = node;                \
} while(0)

#define LL_REMOVE(node, head) do {                        \
    if(node->prev != NULL) node->prev->next = node->next; \
    if(node->next != NULL) node->next->prev = node->prev; \
    if(head == node) head = node->next;                   \
    node->prev = node->next = NULL;                       \
} while(0)

// 线程列表
struct NWORKER
{
    pthread_t thread;
    struct NMANAGER *pool;
    int terminate;
    struct NWORKER *prev;
    struct NWORKER *next;
};

// 任务列表
struct NJOB
{
    void (*func)(struct NJOB *job);
    void *user_data;
    struct NJOB *prev;
    struct NJOB *next;
};

// 管理器
struct NMANAGER 
{
    struct NWORKER *workers;
    struct NJOB *jobs;

    unsigned int total_jobs;

    unsigned int job_count;        // 任务计数变量
    pthread_mutex_t count_mutex;

    pthread_cond_t end_cond;
    pthread_mutex_t end_mutex;

    pthread_cond_t jobs_cond;
    pthread_mutex_t jobs_mutex;   // 任何一个线程在干活之前都需要先获取锁
};

typedef struct NMANAGER nThreadPool;

// 定义线程所做的工作
static void *nThreadCallback(void *arg) 
{ 
    struct NWORKER *worker = (struct NWORKER*) arg;
    while(1) {
        pthread_mutex_lock(&worker->pool->jobs_mutex);     // 干活之前先获取锁
        while(worker->pool->jobs == NULL) {   // 没有任务
            if(worker->terminate) break;
            // condition wait
            pthread_cond_wait(&worker->pool->jobs_cond, &worker->pool->jobs_mutex);
        }
        if(worker->terminate){
            pthread_mutex_unlock(&worker->pool->jobs_mutex);
            break;
        }
        // 从任务列表获取一个任务进行处理
        struct NJOB *job = worker->pool->jobs;
        LL_REMOVE(job, worker->pool->jobs);
        pthread_mutex_unlock(&worker->pool->jobs_mutex);
        job->func(job);

        pthread_mutex_lock(&worker->pool->count_mutex);
        worker->pool->job_count++;
        if (worker->pool->job_count == worker->pool->total_jobs) {
            pthread_cond_signal(&worker->pool->end_cond);
        }
        pthread_mutex_unlock(&worker->pool->count_mutex);

    }
    free(worker);
    pthread_exit(NULL); 
}

// Thread Pool Create
int nThreadPoolCreate(nThreadPool *pool, int numWorkers) 
{
    if(numWorkers < 1) numWorkers = 1;
    if(pool == NULL) return -1;

    memset(pool, 0, sizeof(nThreadPool));

    pthread_cond_t blank_cond = PTHREAD_COND_INITIALIZER;
    memcpy(&pool->jobs_cond, &blank_cond, sizeof(pthread_cond_t));
    memcpy(&pool->end_cond, &blank_cond, sizeof(pthread_cond_t));

    pthread_mutex_t blank_mutex = PTHREAD_MUTEX_INITIALIZER;
    memcpy(&pool->jobs_mutex, &blank_mutex, sizeof(pthread_mutex_t));
    memcpy(&pool->count_mutex, &blank_mutex, sizeof(pthread_mutex_t));
    memcpy(&pool->end_mutex, &blank_mutex, sizeof(pthread_mutex_t));


    for(int i = 0; i<numWorkers; i++) {
        struct NWORKER *worker = (struct NWORKER*)malloc(sizeof(struct NWORKER));  // 创建一个线程
        if(worker == NULL) {
            perror("malloc");
            return -2;
        }
        memset(worker, 0, sizeof(struct NWORKER));
        worker->pool = pool;     // 设置管理器

        int ret = pthread_create(&worker->thread, NULL, nThreadCallback, worker);
        if(ret){
            perror("pthread_create");
            free(worker);
            return -3;
        }
        LL_ADD(worker, pool->workers);   // 加入线程列表
    }
    return 0;
}

// push job to pool 
void nThreadPoolPush(nThreadPool *pool, struct NJOB *job) 
{
    pthread_mutex_lock(&pool->jobs_mutex);
    
    LL_ADD(job, pool->jobs);   // 新任务加入任务列表

    pthread_cond_signal(&pool->jobs_cond);   // 唤醒一个线程去处理

    pthread_mutex_unlock(&pool->jobs_mutex); // 释放锁
}

// destroy pool
int nThreadPoolDestroy(nThreadPool *pool)
{
    struct NWORKER *worker = NULL;
    for(worker = pool->workers; worker != NULL; worker = worker->next) {
        worker->terminate = 1;
    }
    pthread_mutex_lock(&pool->jobs_mutex);
    pthread_cond_broadcast(&pool->jobs_cond);
    pthread_mutex_unlock(&pool->jobs_mutex);
    return 0;
}

#if 1

void print(struct NJOB *job) 
{
    printf("**%d**\n", *((int*)job->user_data));
    unsigned int a = 0;
    for (unsigned int i = 0; i < 10000; i++) {
        for (unsigned int j = 0; j < 10000; j++) {

        }
    }


}

int main() 
{
    nThreadPool *pool = (nThreadPool *)malloc(sizeof(nThreadPool));
    nThreadPoolCreate(pool, 16); // create 16 worker

#define JOB_COUNT 100

    NJOB t[JOB_COUNT];
    pool->total_jobs = JOB_COUNT;
    pool->job_count = 0;

    for(int i = 0; i < JOB_COUNT; i++) {
        t[i].func = print;
        t[i].user_data = (int *)malloc(sizeof(int));
        (*(int*)t[i].user_data) = i;
        nThreadPoolPush(pool, &t[i]);
    }

    if(pool->job_count != JOB_COUNT) {
        pthread_cond_wait(&pool->end_cond, &pool->end_mutex);
        printf("==>%d\n", pool->job_count);
        nThreadPoolDestroy(pool);
    }
}

#endif