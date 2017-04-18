#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <boost/lockfree/spsc_queue.hpp>

#include "lkf.h"
static LKF_LIST(list);
#define uselkf 0

#define nrp 100000
#define nrt 1
uint32_t now();

using namespace boost::lockfree;
static spsc_queue<lkf_node*, capacity<50000000>, fixed_sized<true>> spsc_queue;
static int items = 0;
static struct lkf_node** node_array = NULL;

static int run = 0;
static struct lkf_node* pool = NULL;
static int indx = 0;
static int nr = 0;
static uint32_t t1;

void* writer(void* any)
{
    int* intp = (int *) any;
    while (run == 0) {
        usleep(100);
    }

    for (int i = 0; i < nrp; ++i) {
        int idx = __sync_fetch_and_add(&indx, 1);
        struct lkf_node* nodep = &pool[idx];
#if uselkf
        lkf_node_put(&list, nodep);
#else
        spsc_queue.push(nodep);
#endif
    }
    __sync_sub_and_fetch(&run, 1);
    intp[0] = (int) (now() - t1);
    return NULL;
}

void* reader(void* any)
{
    int* intp = (int *) any;
    while (run == 0) {
        usleep(100);
    }
#if uselkf
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
#else
    while (nr > 0) {
        int n = (int) spsc_queue.pop(node_array, items);
        __sync_sub_and_fetch(&nr, n);
    }
#endif
    intp[0] = (int) (now() - t1);
    return NULL;
}

int main(int argc, char** argv)
{
    int n = 0;
    int used1[1024] = {0};
    for (int i = 0; i < nrt; ++i) {
        pthread_t tid;
        if(0 == pthread_create(&tid, NULL, writer, &used1[i])) {
            ++n;
            pthread_detach(tid);
        }
    }
    nr = nrp * n;

#if !uselkf
    items = nr;
    node_array = (struct lkf_node**) malloc(sizeof(struct lkf_node*) * nr);
    if (node_array == 0) {
        printf("memory\n");
        return 0;
    }
#endif

    pool = (struct lkf_node *) malloc(sizeof(struct lkf_node) * nr);
    if (pool == 0) {
        printf("memory\n");
        return 0;
    }

    int readers = 0;
    int used2[1024] = {0};
    for (int i = 0; i < 1; ++i) {
        pthread_t tid;
        if(0 == pthread_create(&tid, NULL, reader, &used2[i])) {
            ++readers;
            pthread_detach(tid);
        }
    }

    printf("%d writer, %d reader, nr = %d\n", n, readers, nr);
    t1 = now();
    run = n;
    sleep(10);

    int max1 = 0, max2 = 0;
    for (int i = 0; i < n; ++i) {
        if (used1[i] > max1) {
            max1 = used1[i];
        }
    }

    for (int i = 0; i < readers; ++i) {
        if (used2[i] > max2) {
            max2 = used2[i];
        }
    }

    printf("write %d, read %d, nr = %d\n", max1, max2, nr);
    return 0;
}
