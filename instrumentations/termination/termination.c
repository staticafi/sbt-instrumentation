#include <assert.h>

extern void __VERIFIER_error() __attribute__((noreturn));

void __INSTR_fail(void) __attribute__((noreturn));
void __INSTR_fail(void) {
    assert(0 && "infinite loop");
    __VERIFIER_error();
}

void __INSTR_check_nontermination(_Bool c) {
	if (c)
		__INSTR_fail();
}
