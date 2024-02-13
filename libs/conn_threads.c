#include <pthread.h>
#include <stdlib.h>

#define MAX_THREADS 10

// Define a task structure
typedef struct {
    void (*function)(int); // Function pointer to the task function
    int arg; // Argument to be passed to the task function
} Task;

// Define a thread pool structure
typedef struct {
    pthread_t threads[MAX_THREADS];
    Task taskQueue[MAX_THREADS];
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    int count;
} ThreadPool;


// Function executed by each thread in the pool
void* threadFunction(void* arg) {
    ThreadPool* pool = (ThreadPool*)arg;

    while (1) {
        pthread_mutex_lock(&pool->mutex);

        // Wait until a task is available
        while (pool->count == 0) {
            pthread_cond_wait(&pool->condition, &pool->mutex);
        }

        // Get the next task from the queue
        Task task = pool->taskQueue[pool->count - 1];
        pool->count--;

        pthread_mutex_unlock(&pool->mutex);

        // Execute the task
        task.function(task.arg);
    }

    return NULL;
}



// Function to initialize the thread pool
void initializeThreadPool(ThreadPool* pool) {
    // Initialize the mutex and condition variables
    pthread_mutex_init(&pool->mutex, NULL);
    pthread_cond_init(&pool->condition, NULL);

    // Initialize the thread count
    pool->count = 0;

    // Create and spawn the threads in the pool
    for (int i = 0; i < MAX_THREADS; ++i) {
        pthread_create(&pool->threads[i], NULL, threadFunction, (void*)pool);
    }
}

// Function to submit a task to the thread pool
void submitTask(ThreadPool* pool, void (*function)(int), int arg) {
    pthread_mutex_lock(&pool->mutex);

    // Add the task to the queue
    pool->taskQueue[pool->count].function = function;
    pool->taskQueue[pool->count].arg = arg;
    pool->count++;

    // Signal a waiting thread that a new task is available
    pthread_cond_signal(&pool->condition);

    pthread_mutex_unlock(&pool->mutex);
}
