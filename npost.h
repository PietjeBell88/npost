#ifndef NPOST_NPOST_H
#define NPOST_NPOST_H

#include <pthread.h>

#include "diskfile.h"

#define NPOST_VERSION "TESTING"
#define strdup strdup2

typedef struct
{
    // Server info
    char *server;
    int port;
    char *username;
    char *password;

    // Posting info
    char *name;
    char *email;
    char *newsgroups;
    char *comment;

    // Post options
    int threads;
    int lines;
    int linelength;

    // The input files
    int n_input_files;
    diskfile_t *input_files;

    // free() callback
    void (*param_free)( void * );
} npost_param_t;

typedef struct
{
    int filenum;
    int partnum;

    void *next;
} npost_item_t;

typedef struct
{
    // Synched
    npost_param_t param;

    pthread_t *threads;

    pthread_mutex_t *mut;

    npost_item_t *head;
    npost_item_t *tail;
} npost_t;
#endif
