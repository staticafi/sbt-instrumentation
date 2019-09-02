#include <assert.h>

extern void __VERIFIER_error(void) __attribute__((noreturn));

void __INSTR_check_pointer(void* pointer) {
      if (pointer == 0) {
	      __VERIFIER_error();
	      assert(0);
	}
}
