
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
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef lkf_h
#define lkf_h

struct lkf_node {
    struct lkf_node* next;
};

struct lkf_list {
    struct lkf_node root;
    struct lkf_node** tail;
};


#define LKF_INIT(name) {.root = {NULL}, .tail = &(name.root.next)}
#define LKF_LIST(name) \
struct lkf_list name = LKF_INIT(name)

#define INIT_LKF(name) \
do { \
    name->root.next = NULL; \
    name->tail = &(name->root.next); \
} while (0)

static inline void lkf_node_put(struct lkf_list* list, struct lkf_node* node)
{
    struct lkf_node** ptr = __sync_lock_test_and_set(&(list->tail), &(node->next));
    *ptr = node;
}

static inline struct lkf_node* lkf_node_get_one(struct lkf_list* list)
{
    struct lkf_node* head = __sync_lock_test_and_set(&(list->root.next), NULL);
    if (head == NULL) {
        return NULL;
    }

    struct lkf_node* next = head->next;
    if (next != NULL) {
        list->root.next = next;
        head->next = head;
        return head;
    }

    int b = __sync_bool_compare_and_swap(&(list->tail), &(head->next), &(list->root.next));
    if (b) {
        head->next = head;
        return head;
    }

    list->root.next = head;
    return NULL;
}

static inline struct lkf_node* lkf_node_get(struct lkf_list* list)
{
    struct lkf_node* ptr = __sync_lock_test_and_set(&(list->root.next), NULL);
    if (ptr == NULL) {
        return NULL;
    }

    struct lkf_node** last = __sync_lock_test_and_set(&(list->tail), &(list->root.next));
    *last = ptr;
    return *last;
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

#endif
