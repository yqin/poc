#include "common.h"
#include "tslist.h"
#include "arch.h"

tslist_elem_t **elem_pool = NULL;

tslist_t *tslist_create(int nthreads, int nelems)
{
    tslist_t *list = calloc(1, sizeof(*list));
    printf("list = %p\n", list);
    if( NULL == list ) {
        abort();
    }
    list->head = calloc(1, sizeof(*list->head));
    list->tail = list->head;
    printf("list->head = %p, list->tail = %p\n", list->head, list->tail);

    elem_pool = calloc(nthreads, sizeof(*elem_pool));
    int i;
    for(i=0; i<nthreads; i++){
        elem_pool[i] = calloc(nelems, sizeof(*elem_pool[0]));
    }
    return list;
}
void tslist_release(tslist_t *list)
{
    free(list->head);
    list->head = list->tail = NULL;
    free(list);
}


static void _append_to(tslist_t *list, tslist_elem_t *head,
                       tslist_elem_t *tail)
{
    tslist_elem_t *prev = (tslist_elem_t *)atomic_swap((uint64_t*)&list->tail, (uint64_t)tail);
    atomic_wmb();
    prev->next = head;
}

__thread int pool_cnt = 0;
void tslist_append_batch(tslist_t *list, void **ptr, int count)
{
    int i;
    tslist_elem_t head_base = { 0 }, *head = &head_base, *tail = head;
    int tid = get_thread_id();

    for(i = 0; i < count; i++) {
        tslist_elem_t *elem = &elem_pool[tid][pool_cnt++];
        elem->ptr = ptr[i];
        elem->next = NULL;
        tail->next = elem;
        tail = elem;
    }
    if( count ){
        _append_to(list, head->next, tail);
    }
}


void tslist_append(tslist_t *list, void *ptr)
{
    tslist_append_batch(list, ptr, 1);
}

int list_empty_cnt = 0;
int list_first_added_cnt = 0;
int list_extract_one_cnt = 0;
int list_extract_last_cnt = 0;

void tslist_dequeue(tslist_t *list, tslist_elem_t **_elem)
{
    *_elem = NULL;
    if( list->head == list->tail ){
        // the list is empty
        list_empty_cnt++;
        return;
    }

    if(list->head->next == NULL ){
        // Someone is adding a new element, but it is not yet ready
        list_first_added_cnt++;
        return;
    }

    tslist_elem_t *elem = list->head->next;
    if( elem->next ) {
        /* We have more than one elements in the list
         * it is safe to dequeue this elemen as only one thread
         * is allowed to dequeue
         */
        atomic_isync();
        list->head->next = elem->next;
        *_elem = elem;
        /* Terminate element */
        elem->next = NULL;
        /* No need to release in this test suite */
        list_extract_one_cnt++;
        return;
    }

    /* There is a possibility of a race condition:
     * elem->next may still be NULL while a new addition has started that
     * already obtained the pointer to elem.
     */
    list->head->next = NULL;
    if( CAS(&list->tail, elem, list->head) ){
        /* Replacement was successful, this means that list->head was placed
         * as the very first element of the list
         */
    } else {
        /* Replacement failed, this means that other thread is in the process
         * of adding to the list
         */
        /* Wait for the pointer to appear */
        while(NULL != elem->next) {
            atomic_isync();
        }
        /* Hand this element over to the head element */
        list->head->next = elem->next;
    }
    *_elem = elem;
    list_extract_last_cnt++;
    printf("!!!\n");
}

void tslist_append_done(tslist_t *list, int nthreads)
{
    /* This is a dummy function for fake list only */
    printf("list_empty_cnt = %d\n", list_empty_cnt);
    printf("list_first_added_cnt =%d\n", list_first_added_cnt);
    printf("list_extract_one_cnt = %d\n", list_extract_one_cnt);
    printf("list_extract_last_cnt = %d\n", list_extract_last_cnt);
}

tslist_elem_t *tslist_first(tslist_t *list)
{
    return list->head->next;
}

tslist_elem_t *tslist_next(tslist_elem_t *current)
{
    return current->next;
}

/*
Copyright 2019 Artem Y. Polyakov <artpol84@gmail.com>

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may
be used to endorse or promote products derived from this software without specific
prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
OF THE POSSIBILITY OF SUCH DAMAGE.
*/
