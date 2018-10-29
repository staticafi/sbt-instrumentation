#include <assert.h>
#include <stdint.h>

#ifndef NULL
#define NULL ((void*)0)
#endif

/* we do not want to include stdlib.h as it
 * may break build inside Symbiotic, where
 * the include paths are not set to the system's one */
#ifdef __SIZE_TYPE__
typedef __SIZE_TYPE__ size_t;
#else
# if __WORDSIZE == 64
typedef unsigned long int size_t;
#else
typedef unsigned int size_t;
#endif
#endif

extern void *malloc(size_t);
extern void free(void *);

typedef void* rec_id;
typedef uint64_t a_size;

extern void __VERIFIER_error() __attribute__((noreturn));

// record for a memory block
typedef struct {
    rec_id id;
    a_size size;
} rec;


// list of records type
typedef struct rec_list_node{
    int flag;
    rec rec;
    struct rec_list_node *next;
    struct rec_list_node *prev;
} rec_list_node;

rec_list_node *heap_list = NULL;
rec_list_node *stack_list = NULL;
rec_list_node *deallocated_list = NULL;
rec_list_node *globals_list = NULL;

static void __INSTR_list_prepend(rec_list_node *new_node, rec_list_node **head) {
    if ((*head) != NULL)
        (*head)->prev = new_node;
    new_node->next = *head;
    new_node->prev = NULL;
    *head = new_node;
}

static void __INSTR_globals_list_prepend(rec_list_node *node) {
    __INSTR_list_prepend(node, &globals_list);
}

static void __INSTR_heap_list_prepend(rec_list_node *node) {
    __INSTR_list_prepend(node, &heap_list);
}

static void __INSTR_stack_list_prepend(rec_list_node *node) {
    __INSTR_list_prepend(node, &stack_list);
}

static void __INSTR_deallocated_list_prepend(rec_list_node *node) {
    __INSTR_list_prepend(node, &deallocated_list);
}

rec_list_node* __INSTR_node_create(rec_id id, a_size size) {
    rec_list_node *node = (rec_list_node *) malloc(sizeof(rec_list_node));
    node->next = NULL;
    node->prev = NULL;
    node->flag = 0;
    node->rec.id = id;
    node->rec.size = size;

    return node;
}

void __INSTR_rec_create_stack(rec_id id, a_size size) {
    rec_list_node *node = __INSTR_node_create(id, size);
    __INSTR_stack_list_prepend(node);
}

void __INSTR_rec_create_heap(rec_id id, a_size size) {
    rec_list_node *node = __INSTR_node_create(id, size);
    __INSTR_heap_list_prepend(node);
}

void __INSTR_rec_create_deallocated(rec_id id,  a_size size) {
    rec_list_node *node = __INSTR_node_create(id, size);
    __INSTR_deallocated_list_prepend(node);
}

void __INSTR_rec_create_global(rec_id id, a_size size) {
    rec_list_node *node = __INSTR_node_create(id, size);
    __INSTR_globals_list_prepend(node);
}

rec_list_node* __INSTR_list_search(rec_list_node *head, rec_id id) {
    rec_list_node *cur = head;

    while(cur) {
        /* check wether 'id' is a pointer to
         * some memory that we registred, that is if it points
         * to some memory region in range [cur.id, cur.id + size] */
        if (cur->rec.id <= id
             && (cur->rec.id == id || // remove this part of condition
                                       // once we know that rec.size != 0
                 (id - cur->rec.id < cur->rec.size))) {
            return cur;
        }

        cur = cur->next;
    }

    return NULL;
}

void __INSTR_detach_node(rec_list_node *n, rec_list_node **head){

    if (n->prev != NULL) {
        n->prev->next = n->next;
    }
    else {
        *head = n->next;
    }

    if (n->next != NULL) {
        n->next->prev = n->prev;
    }

    n->next = NULL;
    n->prev = NULL;
}

void __INSTR_rec_destroy(rec_list_node *n, rec_list_node **head) {
    __INSTR_detach_node(n, head);
    free(n);
}

void __INSTR_free(rec_id id) {

    // there is no record for NULL
    if (id == 0) {
        return;
    }

    rec_list_node *n = __INSTR_list_search(heap_list, id);

    if (n != NULL && n->rec.id == id) {
        __INSTR_detach_node(n, &heap_list);
        __INSTR_deallocated_list_prepend(n);
        return;
    }

    n = __INSTR_list_search(deallocated_list, id);

    if (n == NULL) {
        assert(0 && "free on non-allocated memory");
        __VERIFIER_error();
    } else {
        assert(0 && "double free");
        __VERIFIER_error();
    }
}

void __INSTR_remember_global(rec_id id, a_size size) {

    rec_list_node *n = __INSTR_list_search(globals_list, id);

    if (n != NULL) {
        // If rec already exists, change the size. This happens because
        // automatons created by alloca instructions are not destroyed at
        // return of the function as they shoud be. This is just a temporary
        // solution.
        n->rec.size = size;
        return;
    } else {
        n = __INSTR_list_search(deallocated_list, id);
        if (n != NULL) {
            __INSTR_detach_node(n, &deallocated_list);
            __INSTR_globals_list_prepend(n);
        }
        else {
            __INSTR_rec_create_global(id, size);
        }
    }
}

void __INSTR_remember(rec_id id, a_size size, int num) {

    rec_list_node *n = __INSTR_list_search(stack_list, id);

    if (n != NULL) {
        // If rec already exists, change the size. This happens because
        // automatons created by alloca instructions are not destroyed at
        // return of the function as they shoud be. This is just a temporary
        // solution.
        n->rec.size = size*num;
        return;
    } else {
        n = __INSTR_list_search(deallocated_list, id);
        if (n != NULL) {
            __INSTR_detach_node(n, &deallocated_list);
            __INSTR_stack_list_prepend(n);
        }
        else {
            __INSTR_rec_create_stack(id, size * num);
        }
    }
}

void __INSTR_remember_malloc_calloc(rec_id id, size_t size, int num ) {
    // there is no record for NULL
    if (id == 0) {
        return;
    }

    rec_list_node *n = __INSTR_list_search(heap_list, id);
    if (n == NULL) {
        n = __INSTR_list_search(deallocated_list, id);
        if (n != NULL) {
            __INSTR_detach_node(n, &deallocated_list);
            __INSTR_heap_list_prepend(n);
        }
        else {
            __INSTR_rec_create_heap(id, size * num);
        }
    }
}

void __INSTR_check_bounds(rec_id addr_a, a_size offa, a_size size, rec_id addr_b, a_size range) {
    int64_t offb = addr_b - addr_a + offa;
    if (offb < 0 || offb + range > size) {
        assert(0 && "dereference out of range");
        __VERIFIER_error();
    }
}

void __INSTR_check(rec_id id, a_size range, rec r) {
    if (range > r.size ||
        /* id - r->id is the offset into memory.
         * Reorder the numbers so that there won't be
         * an overflow */
        ((a_size)(id - r.id)) > r.size - range) {
        assert(0 && "dereference out of range");
        __VERIFIER_error();
    }
}

void __INSTR_check_stack(rec_id id, a_size range) {
    rec_list_node *n = NULL;

    if ((n = __INSTR_list_search(stack_list, id))) {
        __INSTR_check(id, range, n->rec);
    } else {
        /* we register all memory allocations, so if we
         * haven't found the allocation, then this is
         * invalid pointer */
        assert(0 && "invalid pointer dereference");
        __VERIFIER_error();
    }
}

void __INSTR_check_globals(rec_id id, a_size range) {
    rec_list_node *n = NULL;

    if ((n = __INSTR_list_search(globals_list, id))) {
        __INSTR_check(id, range, n->rec);
    } else {
        /* we register all memory allocations, so if we
         * haven't found the allocation, then this is
         * invalid pointer */
        assert(0 && "invalid pointer dereference");
        __VERIFIER_error();
    }
}

void __INSTR_check_heap(rec_id id, a_size range) {
    rec_list_node *n = NULL;

    if ((n = __INSTR_list_search(heap_list, id))) {
        __INSTR_check(id, range, n->rec);
    } else if ((n = __INSTR_list_search(deallocated_list, id))) {
        assert(0 && "dereference on freed memory");
        __VERIFIER_error();
    } else {
        /* we register all memory allocations, so if we
         * haven't found the allocation, then this is
         * invalid pointer */
        assert(0 && "invalid pointer dereference");
        __VERIFIER_error();
    }
}

void __INSTR_check_pointer(rec_id id, a_size range) {
    rec_list_node *n = NULL;

    if ((n = __INSTR_list_search(heap_list, id))) {
        __INSTR_check(id, range, n->rec);
    }
    else if ((n = __INSTR_list_search(stack_list, id))) {
        __INSTR_check(id, range, n->rec);
    }
    else if ((n = __INSTR_list_search(globals_list, id))) {
        __INSTR_check(id, range, n->rec);
    }
    else if ((n = __INSTR_list_search(deallocated_list, id))) {
        assert(0 && "dereference on freed memory");
        __VERIFIER_error();
    } else {
        /* we register all memory allocations, so if we
         * haven't found the allocation, then this is
         * invalid pointer */
        assert(0 && "invalid pointer dereference");
        __VERIFIER_error();
    }
}

void __INSTR_check_bounds_min(rec_id addr_a, a_size min_off, a_size min_space, rec_id addr_b, a_size range) {
    int64_t n = addr_b - addr_a;
    if (-min_off >= n || n + range > min_space) {
        __INSTR_check_pointer(addr_b, range);
    }
}

void __INSTR_check_bounds_min_max(rec_id addr_a, a_size min_off, a_size min_space, a_size max_off, a_size max_space,
                                     rec_id addr_b, a_size range)
{
    int64_t n = addr_b - addr_a;
    if (max_off < n || n + range > max_space) {
        assert(0 && "invalid pointer dereference");
    }
    else if (min_off < n || n + range > min_space) {
        __INSTR_check_pointer(addr_b, range);
    }
}

void __INSTR_check_leaks() {
    rec_list_node *cur = heap_list;

    if (cur != NULL) {
        assert(0 && "memory leak detected");
        __VERIFIER_error();
    }
}

void __INSTR_destroy_list(rec_list_node *head) {
    rec_list_node *cur = head;

    while (cur) {
        rec_list_node *tmp = cur->next;
        free(cur);
        cur = tmp;
    }
}

void __INSTR_destroy_lists() {
    __INSTR_destroy_list(heap_list);
    __INSTR_destroy_list(stack_list);
    __INSTR_destroy_list(deallocated_list);
    __INSTR_destroy_list(globals_list);
}

void __INSTR_check_realloc(rec_id old_id) {
    if (old_id == 0) {
      return;
    }

    rec_list_node *n;
    if ((n = __INSTR_list_search(deallocated_list, old_id))) {
        assert(0 && "realloc on freed memory");
        __VERIFIER_error();
    } else if ((n = __INSTR_list_search(stack_list, old_id))) {
        assert(0 && "realloc on non-dynamically allocated memory");
        __VERIFIER_error();
    }
}

void __INSTR_realloc(rec_id old_id, rec_id new_id, size_t size) {
    if (new_id == 0) {
      return; // if realloc returns null, nothing happens
    }

    if (old_id == 0) {
      __INSTR_rec_create_heap(new_id, size);
      return;
    }

    rec_list_node *n = __INSTR_list_search(heap_list, old_id);

    if (n != NULL) {
        __INSTR_rec_create_heap(new_id, size);
        __INSTR_rec_destroy(n, &heap_list);
    }
}

void __INSTR_set_flag() {
    if (stack_list)
        stack_list->flag++;
}

void __INSTR_destroy(rec_id id) {
    rec_list_node *n = __INSTR_list_search(stack_list, id);
    if (n != NULL) {
        __INSTR_rec_destroy(n, &stack_list);
    }
}

void __INSTR_destroy_allocas() {
    rec_list_node *cur = stack_list;

    while (cur && cur->flag == 0) {
        rec_list_node *tmp = cur->next;
        free(cur);
        cur = tmp;
    }

    if (cur && cur->flag > 0) {
        cur->flag--;
    }

    stack_list = cur;
    if (cur != NULL)
        cur->prev = NULL;
}

void __INSTR_fail() {
    assert(0 && "invalid dereference (null or freed)");
    __VERIFIER_error();

}
