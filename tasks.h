#ifndef TASKS_H
#define TASKS_H

bool start_task(void (*func)(void *payload), void *payload);

#endif
