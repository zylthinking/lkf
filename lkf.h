
/*
 * Copyright (c) 2013, zylthinking
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation and/or
 *    other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef lkf_h
#define lkf_h

#include <stddef.h>

typedef struct lkf_node {
    struct lkf_node* next;
} lkf_node;

typedef struct lkf_list {
    struct lkf_node root;
    struct lkf_node** tail;
} lkf_list;

#define LKF_INIT(name)                                                                             \
    {                                                                                              \
        .root = {NULL}, .tail = &(name.root.next)                                                  \
    }
#define LKF_LIST(name) struct lkf_list name = LKF_INIT(name)

#define INIT_LKF(name)                                                                             \
    do {                                                                                           \
        typeof(name) lkf = name;                                                                   \
        lkf->root.next = NULL;                                                                     \
        lkf->tail = &(lkf->root.next);                                                             \
    } while (0)

static inline void lkf_node_put(struct lkf_list* list, struct lkf_node* head, struct lkf_node* tail)
{
    struct lkf_node** ptr = __atomic_exchange_n(&(list->tail), &(tail->next), __ATOMIC_RELAXED);
    *ptr = head;
}

static inline struct lkf_node* lkf_node_get_one(struct lkf_list* list)
{
    struct lkf_node* head = __atomic_exchange_n(&(list->root.next), NULL, __ATOMIC_RELAXED);
    if (head == NULL) {
        return NULL;
    }

    struct lkf_node* next = head->next;
    if (next != NULL) {
        list->root.next = next;
        head->next = head;
        return head;
    }

    struct lkf_node** nextp = &(head->next);
    int b = __atomic_compare_exchange_n(&(list->tail), &nextp, &(list->root.next), false,
                                        __ATOMIC_RELAXED, __ATOMIC_RELAXED);
    if (b) {
        head->next = head;
        return head;
    }

    list->root.next = head;
    return NULL;
}

static inline struct lkf_node* lkf_node_get(struct lkf_list* list)
{
    struct lkf_node* ptr = __atomic_exchange_n(&(list->root.next), NULL, __ATOMIC_RELAXED);
    if (ptr == NULL) {
        return NULL;
    }

    struct lkf_node** last =
        __atomic_exchange_n(&(list->tail), &(list->root.next), __ATOMIC_RELAXED);
    *last = ptr;
    return (lkf_node*) last;
}

static inline struct lkf_node* lkf_node_next(struct lkf_node* node)
{
    struct lkf_node* ptr = node->next;
    if (ptr == NULL || ptr->next == NULL) {
        return NULL;
    }

    if (ptr == node) {
        return ptr;
    }

    node->next = ptr->next;
    ptr->next = NULL;
    return ptr;
}

static inline lkf_node* lkf_link(struct lkf_node* node)
{
    lkf_node* head = node;
    lkf_node* tail = nullptr;
    while (node != NULL) {
        tail = node;
        node = node->next;
    }
    tail->next = head;
    return tail;
}

#ifdef __linux__
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

static inline void lkf_node_put_wake(struct lkf_list* list, struct lkf_node* node)
{
    struct lkf_node** ptr = __atomic_exchange_n(&(list->tail), &(node->next), __ATOMIC_RELAXED);
    if (ptr == &list->root.next) {
        while (-1 == syscall(__NR_futex, ptr, FUTEX_WAKE, 1, NULL, NULL, 0))
            ;
    }
    *ptr = node;
}

static inline struct lkf_node* lkf_node_get_wait(struct lkf_list* list)
{
    struct lkf_node* ptr = __atomic_exchange_n(&(list->root.next), NULL, __ATOMIC_RELAXED);
    while (ptr == NULL) {
        syscall(__NR_futex, &list->root.next, FUTEX_WAIT, NULL, NULL, NULL, 0);
        ptr = __atomic_exchange_n(&(list->root.next), NULL, __ATOMIC_RELAXED);
    };

    struct lkf_node** last =
        __atomic_exchange_n(&(list->tail), &(list->root.next), __ATOMIC_RELAXED);
    *last = ptr;
    return (lkf_node*) last;
}
#endif

#if 1
typedef struct proc_context {
    struct lkf_list list;
    int stat;
} proc_context;

static int proc_enter(struct proc_context* ctx, struct lkf_node* node)
{
    node->next = NULL;
    lkf_node_put(&ctx->list, node, node);
    int zero = 0;
    int n = (int) __atomic_compare_exchange_n(&ctx->stat, &zero, 1, false, __ATOMIC_RELAXED,
                                              __ATOMIC_RELAXED);
    if (n == 0) {
        return -1;
    }
    return 0;
}

static int proc_leave(struct proc_context* ctx)
{
    ctx->stat = 0;
    __atomic_thread_fence(__ATOMIC_ACQ_REL);
    if (ctx->list.tail == &ctx->list.root.next) {
        return 0;
    }

    int zero = 0;
    int n = (int) __atomic_compare_exchange_n(&ctx->stat, &zero, 1, false, __ATOMIC_RELAXED,
                                              __ATOMIC_RELAXED);
    if (n == 0) {
        return 0;
    }
    return -1;
}
#endif

#endif
