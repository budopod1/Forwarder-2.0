#include <stdio.h>
#include <errno.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <pthread.h>
#include "config.h"

struct ThreadData {
    atomic_bool is_running;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    atomic_bool has_queued_task;
    void (*func)(void *payload);
    void *payload;
};

struct ThreadData threads_data[MAX_THREAD_COUNT] = {
    {false, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, false, NULL, NULL}
};

void thread_cleanup(void *data_ptr) {
    struct ThreadData *data = (struct ThreadData*)data_ptr;
    data->is_running = false;
    data->has_queued_task = false;
    pthread_mutex_unlock(&data->mutex);
}

void *thread_entry(void *data_ptr) {
    struct ThreadData *data = (struct ThreadData*)data_ptr;
    pthread_mutex_lock(&data->mutex);
    pthread_cleanup_push(&thread_cleanup, data_ptr);
    struct timespec wait_until;
    while (true) {
        timespec_get(&wait_until, TIME_UTC);
        wait_until.tv_sec += MAX_THREAD_WAIT;
        
        while (!data->has_queued_task) {
            int wait_result = pthread_cond_timedwait(&data->cond, &data->mutex, &wait_until);
            if (wait_result == ETIMEDOUT) {
                if (data->has_queued_task) {
                    break;
                } else {
                    goto exit_thread;
                }
            } else if (wait_result) {
                printf("Error waiting on condition\n");
                pthread_exit(NULL);
            }
        }

        (*data->func)(data->payload);
        data->has_queued_task = false;
    }

exit_thread:
    pthread_cleanup_pop(1);
    return NULL;
}

bool delegate_task_to_thread(void (*func)(void *payload), void *payload, struct ThreadData *thread_data) {
    pthread_mutex_lock(&thread_data->mutex);
    // The thread could have timed out between this function being called and the mutex
    // being acquired
    if (!thread_data->is_running) {
        pthread_mutex_unlock(&thread_data->mutex);
        return false;
    }
    thread_data->func = func;
    thread_data->payload = payload;
    thread_data->has_queued_task = true;
    pthread_cond_signal(&thread_data->cond);
    pthread_mutex_unlock(&thread_data->mutex);
    return true;
}

bool start_thread_with_task(void (*func)(void *payload), void *payload, struct ThreadData *thread_data) {
    thread_data->has_queued_task = true;
    thread_data->func = func;
    thread_data->payload = payload;
    pthread_t thread;
    pthread_attr_t thread_attr;
    pthread_attr_init(&thread_attr);
    pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);
    int creation_status = pthread_create(&thread, &thread_attr, &thread_entry, thread_data);
    pthread_attr_destroy(&thread_attr);
    bool success = creation_status == 0;
    thread_data->is_running = success;
    return success;
}

bool start_task(void (*func)(void *payload), void *payload) {
    for (int i = 0; i < MAX_THREAD_COUNT; i++) {
        struct ThreadData *thread_data = &threads_data[i];
        if (thread_data->is_running && !thread_data->has_queued_task) {
            if (delegate_task_to_thread(func, payload, thread_data))
                return true;
        } 
    }
    
    for (int i = 0; i < MAX_THREAD_COUNT; i++) {
        struct ThreadData *thread_data = &threads_data[i];
        if (!thread_data->is_running) {
            return start_thread_with_task(func, payload, thread_data);
        } 
    }

    return false;
}
