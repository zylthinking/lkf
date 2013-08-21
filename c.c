
#define _GNU_SOURCE
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>

#include "lkf.h"
static LKF_LIST(list);

#define nrp 100000
#define nrt 500

static int run = 0;
static struct lkf_node* pool = NULL;
static int indx = 0;
static int nr = 0;

void* writer(void* any)
{
    while (run == 0) {
        usleep(100);
    }

    for (int i = 0; i < nrp; ++i) {
        int idx = __sync_fetch_and_add(&indx, 1);
        struct lkf_node* nodep = &pool[idx];
        nodep->next = NULL;
        lkf_node_put(&list, nodep);
    }

    __sync_sub_and_fetch(&run, 1);
    printf("writer exits\n");
    return NULL;
}

void* reader(void* any)
{
    while (run == 0) {
        usleep(100);
    }

    while (nr > 0) {
        struct lkf_node* nodep = lkf_node_get(&list);
        if (nodep == NULL) {
            continue;
        }

        __sync_sub_and_fetch(&nr, 1);
        struct lkf_node* cur = NULL;
        do {
            cur = lkf_node_next(nodep);
            if (cur == nodep) {
                break;
            }

            if (cur != NULL) {
                __sync_sub_and_fetch(&nr, 1);
            }
        } while (1);
    }

    printf("reader exit\n");
    return NULL;
}

void* reader2(void* any)
{
    while (run == 0) {
        usleep(100);
    }

    while (nr > 0) {
        struct lkf_node* nodep = lkf_node_get_one(&list);
        if (nodep == NULL) {
            continue;
        }

        __sync_sub_and_fetch(&nr, 1);
    }
    printf("reader2 exit\n");
    return NULL;
}

int main(int argc, char** argv)
{
    int n = 0;
    for (int i = 0; i < nrt; ++i) {
        pthread_t tid;
        if(0 == pthread_create(&tid, NULL, writer, NULL)) {
            ++n;
            pthread_detach(tid);
        }
    }

    nr = nrp * n;
    pool = (struct lkf_node *) malloc(sizeof(struct lkf_node) * nr);
    if (pool == 0) {
        printf("memory\n");
        return 0;
    }

    int readers = 0;
    for (int i = 0; i < 500; ++i) {
        pthread_t tid;
        if(0 == pthread_create(&tid, NULL, reader2, NULL)) {
            ++readers;
            pthread_detach(tid);
        }
    }

    printf("run = %d\n", run);
    printf("%d writer, %d reader, nr = %d\n", n, readers + 1, nr);
    run = n;

    time_t t1 = time(0);
    if (nr > 0) {
        //reader(0);
    }

    while (1) {
        usleep(10);
        struct lkf_node* nodep = 0;//lkf_node_get(&list);
        time_t t2 = time(0);
        if (t2 > t1 + 3) {
            printf("%d %d %p\n", nr, indx, nodep);
            t1 = t2;
        }
    }
    return 0;
}
