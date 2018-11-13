#include <assert.h>

extern void __VERIFIER_error() __attribute__((noreturn));

void __INSTR_fail() {
    assert(0 && "infinite loop");
    __VERIFIER_error();
}
