#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

typedef void* rec_id;
typedef uint64_t a_size;

extern void __VERIFIER_error() __attribute__((noreturn));

typedef enum {
	REC_STATE_ALLOCATED,
	REC_STATE_FREED,
	REC_STATE_NONE
} rec_state;

// record for a memory block
typedef struct {
	rec_id id;
	a_size size;
	rec_state state;
} rec;


// list of records type
typedef struct rec_list_node{
	rec *rec;
	struct rec_list_node *next;
} rec_list_node;

rec_list_node *rec_list = NULL;
rec_list_node *allocas_list = NULL;

static void __INSTR_list_prepend(rec_list_node *new_node, rec_list_node **head) {
	new_node->next = *head;
	*head = new_node;
}

static void __INSTR_rec_list_prepend(rec_list_node *node) {
	__INSTR_list_prepend(node, &rec_list);
}

static void __INSTR_allocas_list_prepend(rec_list_node *node) {
	__INSTR_list_prepend(node, &allocas_list);
}

rec_list_node* __INSTR_node_create(rec_id id, rec_state state, a_size size) {
	rec *new_rec = (rec *) malloc(sizeof(rec));
	new_rec->id = id;
	new_rec->state = state;
	new_rec->size = size;

	rec_list_node *node = (rec_list_node *) malloc(sizeof(rec_list_node));
	node->next = NULL;
	node->rec = new_rec;

	return node;
}

rec* __INSTR_rec_create_stack(rec_id id, rec_state state, a_size size) {
	rec_list_node *node = __INSTR_node_create(id, state, size);

	__INSTR_allocas_list_prepend(node);

	return node->rec;
}

rec* __INSTR_rec_create_heap(rec_id id, rec_state state, a_size size) {
	rec_list_node *node = __INSTR_node_create(id, state, size);

	__INSTR_rec_list_prepend(node);

	return node->rec;
}
rec* __INSTR_list_search(rec_list_node *head, rec_id id) {
  	rec_list_node *cur = head;

	while(cur) {
		/* check wether 'id' is a pointer to
		 * some memory that we registred, that is if it points
		 * to some memory region in range [cur.id, cur.id + size] */
		if (cur->rec->id <= id
		     && (cur->rec->id == id || // remove this part of condition
		                               // once we know that rec->size != 0
		         (id - cur->rec->id < cur->rec->size))) {
			return cur->rec;
		}

		cur = cur->next;
	}

	return NULL;
}

rec* __INSTR_rec_list_search(rec_id id) {
	return __INSTR_list_search(rec_list, id);
}

rec* __INSTR_allocas_list_search(rec_id id) {
	return __INSTR_list_search(allocas_list, id);
}

void __INSTR_free(rec_id id) {

	// there is no record for NULL
	if (id == 0) {
		return;
	}

	rec *m = __INSTR_rec_list_search(id);
	
	// Memory was already freed - double free error
	if (m != NULL && m->state == REC_STATE_FREED) {
	    assert(0 && "double free");
		__VERIFIER_error();
	}
	
	if (m == NULL || (m != NULL && m->id != id)) {
		assert(0 && "free on non-allocated memory");
		__VERIFIER_error();
	} else {
		m->state = REC_STATE_FREED;
	}
}

void __INSTR_remember(rec_id id, a_size size, int num) {
	
	rec *m = __INSTR_allocas_list_search(id);

	if(m != NULL){
		// If rec already exists, change the size. This happens because
		// automatons created by alloca instructions are not destroyed at
		// return of the function as they shoud be. This is just a temporary
		// solution. 
		m->state = REC_STATE_NONE;
		m->size = size*num;
		return;
	}
 
	rec *new_rec = __INSTR_rec_create_stack(id, REC_STATE_NONE, size * num);

	rec_list_node *node = (rec_list_node *) malloc(sizeof(rec_list_node));
	node->next = NULL;
	node->rec = new_rec;

	__INSTR_allocas_list_prepend(node);
}

void __INSTR_remember_malloc_calloc(rec_id id, size_t size, int num ) {
	// there is no record for NULL
	if (id == 0) {
		return;
	}

	rec *m = __INSTR_rec_list_search(id);
	if (m != NULL) {
		m->state = REC_STATE_ALLOCATED;
	} else {
		m = __INSTR_rec_create_heap(id, REC_STATE_ALLOCATED, size * num);
	}
}

/* void __INSTR_check_str_length(rec_id dest, rec_id source) {
	rec *rd = __INSTR_rec_list_search(dest);
	rec *rs = __INSTR_rec_list_search(source);

	if (rd != NULL && rs != NULL) {
		if (rd->size < rs->size) {
			assert(0 && "strcpy out of range");
			__VERIFIER_error();
		}

		// this memory was already freed
		if(rs->state == REC_STATE_FREED || rd->state == REC_STATE_FREED) {
			assert(0 && "strcpy on freed memory");
			__VERIFIER_error();
		}
	} else {
		// we register all memory allocations, so if we
		// haven't found the allocation, then this is
		// invalid pointer 
		assert(0 && "strcpy on invalid pointer");
		__VERIFIER_error();
	}
}*/

void __INSTR_check(rec_id id, a_size range, rec *r) {
	if (range > r->size ||
	    /* id - r->id is the offset into memory.
	     * Reorder the numbers so that there won't be
	     * an overflow */
	    ((a_size)(id - r->id)) > r->size - range) {
		assert(0 && "dereference out of range");
		__VERIFIER_error();
	}

	// this memory was already freed
	if(r->state == REC_STATE_FREED) {
		assert(0 && "dereference on freed memory");
		__VERIFIER_error();
	} 
}

void __INSTR_check_pointer(rec_id id, a_size range) {
	rec *r = NULL;

	if ((r = __INSTR_rec_list_search(id))) {
		__INSTR_check(id, range, r);
	}
	else if ((r = __INSTR_allocas_list_search(id))) {
		__INSTR_check(id, range, r);
	} else {
		/* we register all memory allocations, so if we
		 * haven't found the allocation, then this is
		 * invalid pointer */
		assert(0 && "invalid pointer dereference");
		__VERIFIER_error();
	}

}

void __INSTR_rec_destroy(rec_id id, rec_list_node *head) {
    rec_list_node *cur = head;

    if(cur && cur->rec->id == id) {
        rec_list_node *new_head = cur->next;
        free(cur->rec);
        free(cur);
        rec_list = new_head;
        return;
    }

  	while((cur) && (cur->next)) {
  	  if ((*cur->next->rec).id == id) {
            rec_list_node *tmp = cur->next->next;
            free(cur->next->rec);
            free(cur->next);
	    cur->next = tmp;
	    return;
	  }
	  cur = cur->next;
	}
}

void __INSTR_rec_destroy_heap(rec_id id){
	__INSTR_rec_destroy(id, rec_list);
}

void __INSTR_rec_destroy_stack(rec_id id){
	__INSTR_rec_destroy(id, allocas_list);
}

void __INSTR_check_leaks() {
    rec_list_node *cur = rec_list;

    while(cur) {
        rec_list_node *tmp = cur->next;
		if (cur->rec->state == REC_STATE_ALLOCATED){
			assert(0 && "memory leak detected");
			__VERIFIER_error();
		}
        cur = tmp;
    }
}

void __INSTR_destroy_list(rec_list_node *head) {
    rec_list_node *cur = head;

    while(cur) {
        rec_list_node *tmp = cur->next;
        free(cur->rec);
        free(cur);
        cur = tmp;
    }
}

void __INSTR_destroy_lists() {
	__INSTR_destroy_list(rec_list);
	__INSTR_destroy_list(allocas_list);
}

void __INSTR_realloc(rec_id old_id, rec_id new_id, size_t size) {
	if(new_id == 0){
	  return; //if realloc returns null, nothing happens
	}
	
	rec *m = NULL;
	
	if(old_id == 0){
	  m = __INSTR_rec_create_heap(new_id, REC_STATE_ALLOCATED, size);
	  return;
	}

	m = __INSTR_rec_list_search(old_id);
	
	if (m != NULL) {
		if(m->state == REC_STATE_FREED){
		    assert(0 && "realloc on memory that has already been freed");
		    __VERIFIER_error();
		}
		
		rec *new_rec = __INSTR_rec_create_heap(new_id, REC_STATE_ALLOCATED, size);

		rec_list_node *node = (rec_list_node *) malloc(sizeof(rec_list_node));
		node->next = NULL;
		node->rec = new_rec;

		__INSTR_rec_list_prepend(node);
		__INSTR_rec_destroy_heap(old_id);		
	}
	else{
		assert(0 && "realloc on not allocated memory");
		__VERIFIER_error();
	}
}
