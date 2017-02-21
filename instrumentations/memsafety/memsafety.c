#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

typedef void* fsm_id;
typedef uint64_t a_size;

extern void __VERIFIER_error() __attribute__((noreturn));

typedef enum {
	FSM_STATE_ALLOCATED,
	FSM_STATE_FREED,
	FSM_STATE_NONE
} fsm_state;

// finite state machine
typedef struct {
	fsm_id id;
	a_size size;
	fsm_state state;
} fsm;


// FSM list type
typedef struct fsm_list_node{
	fsm *fsm;
	struct fsm_list_node *next;
} fsm_list_node;

fsm_list_node *fsm_list = NULL;

static void __INSTR_fsm_list_prepend(fsm_list_node *node) {
	node->next = fsm_list;
	fsm_list = node;
}

fsm* __INSTR_fsm_create(fsm_id id, fsm_state state) {
	fsm *new_fsm = (fsm *) malloc(sizeof(fsm));
	new_fsm->id = id;
	new_fsm->state = state;

	fsm_list_node *node = (fsm_list_node *) malloc(sizeof(fsm_list_node));
	node->next = NULL;
	node->fsm = new_fsm;

	__INSTR_fsm_list_prepend(node);

	return new_fsm;
}

fsm* __INSTR_fsm_list_search(fsm_id id) {
  	fsm_list_node *cur = fsm_list;

	while(cur) {
		/* check wether 'id' is a pointer to
		 * some memory that we registred, that is if it points
		 * to some memory region in range [cur.id, cur.id + size] */
		if (cur->fsm->id <= id
		     && (cur->fsm->id == id || // remove this part of condition
		                               // once we know that fsm->size != 0
		         (id - cur->fsm->id < cur->fsm->size))) {
			return cur->fsm;
		}

		cur = cur->next;
	}

	return NULL;
}

// FSM manipulation
void __INSTR_free(fsm_id id) {

	// there is no FSM for NULL
	if (id == 0) {
		return;
	}

	fsm *m = __INSTR_fsm_list_search(id);
	
	// Memory was already freed - double free error
	if (m != NULL && m->state == FSM_STATE_FREED) {
	    assert(0 && "double free");
		__VERIFIER_error();
	}
	
	if (m == NULL || (m != NULL && m->id != id)) {
		assert(0 && "free on non-allocated memory");
		__VERIFIER_error();
	} else {
		m->state = FSM_STATE_FREED;
	}

	
}

void __INSTR_remember(fsm_id id, a_size size, int num) {
	
	fsm *m = __INSTR_fsm_list_search(id);

	if(m != NULL){
		// If fsm already exists, change the size. This happens because
		// automatons created by alloca instructions are not destroyed at
		// return of the function as they shoud be. This is just a temporary
		// solution. 
		m->state = FSM_STATE_NONE;
		m->size = size*num;
		return;
	}
 
	fsm *new_rec = (fsm *) malloc(sizeof(fsm));
	new_rec->id = id;
	new_rec->size = size * num;
        new_rec->state = FSM_STATE_NONE;

	fsm_list_node *node = (fsm_list_node *) malloc(sizeof(fsm_list_node));
	node->next = NULL;
	node->fsm = new_rec;

	__INSTR_fsm_list_prepend(node);
}

void __INSTR_remember_malloc_calloc(fsm_id id, size_t size, int num ) {
	// there is no FSM for NULL
	if (id == 0) {
		return;
	}

	fsm *m = __INSTR_fsm_list_search(id);
	if (m != NULL) {
		m->state = FSM_STATE_ALLOCATED;
	} else {
		m = __INSTR_fsm_create(id, FSM_STATE_ALLOCATED);
	}

	if (m != NULL) {
	        m->size = size * num;
	}
}

void __INSTR_check_str_length(fsm_id dest, fsm_id source) {
	fsm *rd = __INSTR_fsm_list_search(dest);
	fsm *rs = __INSTR_fsm_list_search(source);

	if (rd != NULL && rs != NULL) {
		if (rd->size < rs->size) {
			assert(0 && "strcpy out of range");
			__VERIFIER_error();
		}

		// this memory was already freed
		if(rs->state == FSM_STATE_FREED || rd->state == FSM_STATE_FREED) {
			assert(0 && "strcpy on freed memory");
			__VERIFIER_error();
		}
	} else {
		/* we register all memory allocations, so if we
		 * haven't found the allocation, then this is
		 * invalid pointer */
		assert(0 && "strcpy on invalid pointer");
		__VERIFIER_error();
	}
}

void __INSTR_check_pointer(fsm_id id, a_size range) {
	fsm *r = __INSTR_fsm_list_search(id);

	if (r != NULL) {
		if (range > r->size ||
		    /* id - r->id is the offset into memory.
		     * Reorder the numbers so that there won't be
		     * an overflow */
		    ((a_size)(id - r->id)) > r->size - range) {
			assert(0 && "dereference out of range");
			__VERIFIER_error();
		}

		// this memory was already freed
		if(r->state == FSM_STATE_FREED) {
			assert(0 && "dereference on freed memory");
			__VERIFIER_error();
		}
	} else {
		/* we register all memory allocations, so if we
		 * haven't found the allocation, then this is
		 * invalid pointer */
		assert(0 && "invalid pointer dereference");
		__VERIFIER_error();
	}
}

void __INSTR_fsm_destroy(fsm_id id) {
    fsm_list_node *cur = fsm_list;

    if(cur && cur->fsm->id == id) {
        fsm_list_node *newHead = cur->next;
        free(cur->fsm);
        free(cur);
        fsm_list = newHead;
        return;
    }

  	while((cur) && (cur->next)) {
  	  if ((*cur->next->fsm).id == id) {
            fsm_list_node *tmp = cur->next->next;
            free(cur->next->fsm);
            free(cur->next);
	    cur->next = tmp;
	    return;
	  }
	  cur = cur->next;
	}

}

void __INSTR_fsm_list_destroy() {
    fsm_list_node *cur = fsm_list;

    while(cur) {
        fsm_list_node *tmp = cur->next;
	if (cur->fsm->state == FSM_STATE_ALLOCATED){
		assert(0 && "memory leak detected");
		__VERIFIER_error();
	}

        free(cur->fsm);
        free(cur);
        cur = tmp;
    }
}

void __INSTR_realloc(fsm_id old_id, fsm_id new_id, size_t size) {
	if(new_id == 0){
	  return; //if realloc returns null, nothing happens
	}
	
	fsm *m = NULL;
	
	if(old_id == 0){
	  m = __INSTR_fsm_create(new_id, FSM_STATE_ALLOCATED);
	  m->size = size;
	  return;
	}

	m = __INSTR_fsm_list_search(old_id);
	
	if (m != NULL) {
		if(m->state == FSM_STATE_FREED){
		    assert(0 && "realloc on memory that has already been freed");
		    __VERIFIER_error();
		}
		
		fsm *new_rec = (fsm *) malloc(sizeof(fsm));
		new_rec->id = new_id;
		new_rec->size = size;
		new_rec->state = FSM_STATE_ALLOCATED;

		fsm_list_node *node = (fsm_list_node *) malloc(sizeof(fsm_list_node));
		node->next = NULL;
		node->fsm = new_rec;

		__INSTR_fsm_list_prepend(node);
		__INSTR_fsm_destroy(old_id);		
	}
	else{
		assert(0 && "realloc on not allocated memory");
		__VERIFIER_error();
	}
}
