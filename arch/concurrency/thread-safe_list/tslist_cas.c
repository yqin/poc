#include "common.h"
#include "tslist.h"
#include "x86.h"

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


static void _append_to(tslist_t *list, tslist_elem_t *head)
{
    int count = 0;
    // Get approximate tail
    tslist_elem_t *last = list->tail;

    // Search for the exact tail and append to it
    while( 1 ) {
        while(last->next != NULL ){
            last = last->next;
            count++;
        }
        int64_t oldval = 0;
        if( CAS((int64_t*)&last->next, &oldval, (int64_t)head) ){
            // If a pointer to the element was successfully added
            // set the new tail
            oldval = (int64_t)list->tail;
            if( count > nskip ) {
                // Only adjust the tale once in a while
                CAS((int64_t*)&list->tail, &oldval, (int64_t)head);
            }
            break;
        }
    }
}

__thread int pool_cnt = 0;
void tslist_append_batch(tslist_t *list, void **ptr, int count)
{
    int i;
    tslist_elem_t head_base = { 0 }, *head = &head_base;
    int tid = get_thread_id();

    for(i = 0; i < count; i++) {
        tslist_elem_t *elem = &elem_pool[tid][pool_cnt++];
        elem->ptr = ptr[i];
        elem->next = head->next;
        head->next = elem;
    }
    _append_to(list, head->next);
}


void tslist_append(tslist_t *list, void *ptr)
{
    tslist_append_batch(list, ptr, 1);
}

void tslist_append_done(tslist_t *list, int nthreads)
{
    /* This is a dummy function for fake list only */
}

tslist_elem_t *tslist_first(tslist_t *list)
{
    return list->head->next;
}

tslist_elem_t *tslist_next(tslist_elem_t *current)
{
    return current->next;
}