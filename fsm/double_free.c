
typedef fsm_id int; // TODO: Is this size sufficient?

typedef enum {
	FSM_STATE_ALLOCATED,
	FSM_STATE_FREED,
	FSM_STATE_ERROR
} fsm_state;

typedef enum {
	FSM_ALPHABET_FREE,
	FSM_ALPHABET_MALLOC,
} fsm_alphabet;

// finite state machine
typedef struct {
	fsm_id id; 
	fsm_state state;
} fsm;


// FSM list type
typedef struct {
	fsm *fsm; 
	fsm *next;
} fsm_list_node;

fsm_list_node *fsm_list = NULL;

fsm* fsm_create(fsm_id id, fsm_state state) {
	fsm *new_fsm = (fsm *) malloc(sizeof(fsm*));
	new_fsm.id = id;
	new_fsm.state = state;

	fsm_list_node *node = (fsm_list_node *) malloc(sizeof(fsm_list_node*));
	node.next = NULL;
	node.cur = new_fsm;

	fsm_list_append(node);

	return new_fsm;
}

void fsm_list_append(fsm_list_node *node) {
	fsm_list_node *cur = fsm_list;
	for (; *cur && *(cur.next); cur = cur.next);
	
	if (cur == NULL) {
		cur = node;
	} else {
		cur.next = node;
	}
}

fsm *fsm_list_search(fsm_id id) {
	for (; *cur; cur = cur.next) {
		if (*cur.id == id) {
			return cur->fsm;
		}
	}
	return NULL;
}

// FSM manipulation

fsm_state fsm_transition_table[3][2]; // TODO: 3 vs sizeof(fsm_state)?

void fsm_change_state(fsm_id id, fsm_alphabet action) {
	fsm *m = fsm_list_search(fsm_id);
	if (m != NULL) {
		m.state = fsm_transition_table[m.state][action];
	} else {
		if (action == FSM_ALPHABET_FREE) {
			// TODO: ERROR
		}
		fsm_list_create(fsm_id, FSM_STATE_ALLOCATED); 	
	}

	if (m.state == ERROR) {
		// TODO
	}
}
