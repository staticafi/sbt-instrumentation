#include <stdlib.h>
#include <assert.h>

void __INSTR_check_pointer(void* pointer) {
      if(pointer == NULL) {
      assert(0);
      __VERIFIER_error();
    }
}
