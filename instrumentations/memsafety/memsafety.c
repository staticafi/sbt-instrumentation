#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

typedef int8_t* fsm_id;
typedef uint64_t a_size;

typedef enum {
	FSM_STATE_ALLOCATED,
	FSM_STATE_FREED,
	FSM_STATE_ERROR,
	FSM_STATE_NONE
} fsm_state;

typedef enum {
	FSM_ALPHABET_FREE,
	FSM_ALPHABET_MALLOC,
} fsm_alphabet;

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

void __INSTR_fsm_list_append(fsm_list_node *node) {
	fsm_list_node *cur = fsm_list;

	while((cur) && (cur->next)) {
	  cur = cur->next;
	}

	if (cur == NULL) {
		cur = node;
		fsm_list = node;
	} else {
		cur->next = node;
	}
}

fsm* __INSTR_fsm_create(fsm_id id, fsm_state state) {
	fsm *new_fsm = (fsm *) malloc(sizeof(fsm));
	new_fsm->id = id;
	new_fsm->state = state;

	fsm_list_node *node = (fsm_list_node *) malloc(sizeof(fsm_list_node));
	node->next = NULL;
	node->fsm = new_fsm;

	__INSTR_fsm_list_append(node);

	return new_fsm;
}

fsm* __INSTR_fsm_list_search(fsm_id id) {
  	fsm_list_node *cur = fsm_list;

	while(cur) {
		/* check wether 'id' is a pointer to
		 * some memory that we registred, that is if it points
		 * to some memory region in range [cur.id, cur.id + size] */
		if (cur->fsm->id <= id
		     && (id - cur->fsm->id < cur->fsm->size)) {
			return cur->fsm;
		}

		cur = cur->next;
	}

	return NULL;
}

// FSM manipulation

fsm_state fsm_transition_table[4][2] = {{ FSM_STATE_FREED, FSM_STATE_ALLOCATED }, // allocated
                                        { FSM_STATE_ERROR, FSM_STATE_ALLOCATED }, // freed
                                        { FSM_STATE_ERROR, FSM_STATE_ERROR }, // error
					{ FSM_STATE_NONE, FSM_STATE_NONE}};

void __INSTR_fsm_change_state(fsm_id id, fsm_alphabet action) {

	// there is no FSM for NULL
	if (id == 0) {
		return;
	}

	fsm *m = __INSTR_fsm_list_search(id);
	if (m != NULL) {
		if(action == FSM_ALPHABET_FREE && m->id != id){
			assert(0 && "free on non-allocated memory");
			__VERIFIER_error();
		}
		m->state = fsm_transition_table[m->state][action];
	} else {
		if (action == FSM_ALPHABET_FREE) {
			assert(0 && "free on non-allocated memory");
			__VERIFIER_error();
		}
		m = __INSTR_fsm_create(id, FSM_STATE_ALLOCATED);
	}

	if (m != NULL && m->state == FSM_STATE_ERROR) {
	        assert(0 && "double free");
		__VERIFIER_error();
	}
}

fsm* __INSTR_remember(fsm_id id, a_size size, int num) {
	fsm *new_rec = (fsm *) malloc(sizeof(fsm));
	new_rec->id = id;
	new_rec->size = size * num;
        new_rec->state = FSM_STATE_NONE;

	fsm_list_node *node = (fsm_list_node *) malloc(sizeof(fsm_list_node));
	node->next = NULL;
	node->fsm = new_rec;

	__INSTR_fsm_list_append(node);

	return new_rec;
}

void __INSTR_remember_malloc_calloc(fsm_id id, size_t size, int num ) {
	// there is no FSM for NULL
	if (id == 0) {
		return;
	}

	fsm *m = __INSTR_fsm_list_search(id);
	if (m != NULL) {
		m->state = fsm_transition_table[m->state][FSM_ALPHABET_MALLOC];
	} else {
		m = __INSTR_fsm_create(id, FSM_STATE_ALLOCATED);
	}

	if (m != NULL) {
	        m->size = size * num;
	}
}

void __INSTR_check_range(fsm_id id, int range) {
	fsm *r = __INSTR_fsm_list_search(id);

	if (r != NULL) {
		if (range > r->size ||
		    /* id - r->id is the offset into memory.
		     * Reorder the numbers so that there won't be
		     * an overflow */
		    ((a_size)(id - r->id)) > r->size - range) {
			assert(0 && "memset/memcpy out of range");
			__VERIFIER_error();
		}

		// this memory was already freed
		if(r->state == FSM_STATE_FREED) {
			assert(0 && "memset/memcpy on freed memory");
			__VERIFIER_error();
		}
	} else {
		/* we register all memory allocations, so if we
		 * haven't found the allocation, then this is
		 * invalid pointer */
		assert(0 && "memset/memcpy on invalid pointer");
		__VERIFIER_error();
	}
}

void __INSTR_check_load_store(fsm_id id, a_size range) {
	fsm *r = __INSTR_fsm_list_search(id);

	if (r != NULL) {
		/* (id - rec->id) is the offset into allocated memory
		 * it must be possitive, since id >= rec->id */
		if ((a_size)(id - r->id + range) > r->size) {
			assert(0 && "load or store out of range");
			__VERIFIER_error();
		}

		// this memory was already freed
		if(r->state == FSM_STATE_FREED) {
			assert(0 && "load or store on invalid pointer");
			__VERIFIER_error();
		}
	} else {
		/* we register all memory allocations, so if we
		 * haven't found the allocation, then this is
		 * invalid pointer */
		assert(0 && "load or store on invalid pointer");
		__VERIFIER_error();
	}
}

void __INSTR_check_pointer(fsm_id id) {
	fsm *r = __INSTR_fsm_list_search(id);

	if (r == NULL) {
		/* we register all memory allocations, so if we
		 * haven't found the allocation, then this is
		 * invalid pointer */
		assert(0 && "dereference of invalid pointer");
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
