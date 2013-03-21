#pragma once

/*
 * Writer manages the writing thread, and queue of messages to process.
 */

#include <pthread.h>
#include <stdint.h>

typedef struct _Writer
{
    pthread_t thread;
    int outfd;                /* File to write */
    int queue[2];             /* Socket pair to be used as a synchronization
                                 queue */
} Writer;

/* Create a writer to work with the given file */
Writer *writer_init(char const *fname);
/* Purge the queue and wait until the thread finishes */
void writer_close(Writer *writer);

/* Post the given message into the queue for processing */
struct Message;
int writer_post(Writer *writer, struct Message *message);
