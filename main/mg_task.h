#pragma once

#define MG_LISTEN_ADDR "80"
#define MG_TASK_STACK_SIZE 4 * 1024
#define MG_TASK_PRIORITY 1
#define MG_POLL 1000

void mg_task(void *);
