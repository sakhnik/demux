#pragma once

#include <pthread.h>
#include <stdint.h>

typedef struct _Writer
{
    pthread_t thread;
    volatile int exit_flag;
    FILE *file;
} Writer;


Writer *writer_init(char const *fname);

void writer_close(Writer *writer);
void writer_wait_close(Writer *writer);

struct Message;
int writer_post(Writer *writer, struct Message *message);
