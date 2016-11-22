#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

typedef int8_t* rec_id;
typedef uint64_t a_size;

// structure for records
typedef struct {
	rec_id id;
	a_size size;
} rec;


// records list type
typedef struct rec_list_node{
	rec *record;
	struct rec_list_node *next;
} rec_list_node;

rec_list_node *rec_list = NULL;

void __INSTR_rec_list_append(rec_list_node *node) {
	rec_list_node *cur = rec_list;

	while((cur) && (cur->next)) {
	  cur = cur->next;
	}

	if (cur == NULL) {
		cur = node;
		rec_list = node;
	} else {
		cur->next = node;
	}
}

rec* __INSTR_remember(rec_id id, a_size size, a_size num) {
	rec *new_rec = (rec *) malloc(sizeof(rec));
	new_rec->id = id;
	new_rec->size = size * num;

	rec_list_node *node = (rec_list_node *) malloc(sizeof(rec_list_node));
	node->next = NULL;
	node->record = new_rec;

	__INSTR_rec_list_append(node);

	return new_rec;
}

rec* __INSTR_remember_malloc(rec_id id, int size) {
	rec *new_rec = (rec *) malloc(sizeof(rec));
	new_rec->id = id;
	new_rec->size = size;

	rec_list_node *node = (rec_list_node *) malloc(sizeof(rec_list_node));
	node->next = NULL;
	node->record = new_rec;

	__INSTR_rec_list_append(node);

	return new_rec;
}

rec* __INSTR_rec_list_search(rec_id id) {
  	rec_list_node *cur = rec_list;

  	while(cur) {
		/* check wether 'id' is a pointer to
		 * some memory that we registred, that is if it points
		 * to some memory region in range [cur.id, cur.id + size] */
		if ((*cur->record).id <= id
		     && id < (*cur->record).id + (*cur->record).size) {
			return cur->record;
		}
	  cur = cur->next;
	}

	return NULL;
}

void __INSTR_check_range(rec_id id, int range) {
	rec *r = __INSTR_rec_list_search(id);

	if (r != NULL) {
		/* (id - rec->id) is the offset into allocated memory
		 * it must be possitive, since id >= rec->id */
		if ((a_size)(id - r->id + range) > r->size) {
			assert(0 && "memset out of range");
			__VERIFIER_error();
		}
	} else {
		/* we register all memory allocations, so if we
		 * haven't found the allocation, then this is
		 * invalid pointer */
		assert(0 && "memset on invalid pointer");
		__VERIFIER_error();
	}
}

void __INSTR_check_pointer(rec_id id) {
	rec *r = __INSTR_rec_list_search(id);

	if (r == NULL) {
		/* we register all memory allocations, so if we
		 * haven't found the allocation, then this is
		 * invalid pointer */
		assert(0 && "dereference of invalid pointer");
		__VERIFIER_error();
	}
}

void __INSTR_rec_destroy(rec_id id) {
    rec_list_node *cur = rec_list;

    if(cur && cur->record->id == id) {
        rec_list_node *newHead = cur->next;
        free(cur->record);
        free(cur);
        rec_list = newHead;
        return;
    }

  	while((cur) && (cur->next)) {
  	  if ((*cur->next->record).id == id) {
            rec_list_node *tmp = cur->next->next;
            free(cur->next->record);
            free(cur->next);
	    cur->next = tmp;
	    return;
	  }
	  cur = cur->next;
	}

}

void __INSTR_rec_list_destroy() {
    rec_list_node *cur = rec_list;

    while(cur) {
        rec_list_node *tmp = cur->next;
        free(cur->record);
        free(cur);
        cur = tmp;
    }
}


