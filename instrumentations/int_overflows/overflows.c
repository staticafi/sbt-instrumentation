#include <assert.h>
#include <limits.h>

void __INSTR_check_add(int x, int y) {
      if((x > 0) && (y > 0) && (x > INT_MAX - y)) {
	      assert(0 && "Addition: integer overflow!");
      }

      if((x < 0) && (y < 0) && (x < INT_MIN - y)) {
	      assert(0 && "Addition: integer underflow!");
      }
}

void __INSTR_check_sub(int x, int y) {
      if((y > 0) && (x < INT_MIN +  y)) {
	      assert(0 && "Substraction: integer underflow!");
      }

      if((y < 0) && (x > INT_MAX + y)) {
	      assert(0 && "Substraction: integer overflow!");
      }
}

void __INSTR_check_mul(int x, int y) {
    if (x > INT_MAX / y) {
	    assert(0 && "Multiplication: integer overflow!");
    }
    if ((x < INT_MIN / y)) {
        assert(0 && "Multiplication: integer unerflow!");
    }

    // 2's complement detection
    #if (INT_MIN != -INT_MAX)
      if ((x == -1) && (y == INT_MIN))
      if ((y == -1) && (x == INT_MIN))
    #endif
}

int __INSTR_check_div(int op1, int op2) {
  if (op2 == 0) {
    assert(0 && "Division by zero!");
  }
  // 2's complement detection
  #if (INT_MIN != -INT_MAX)
    if (op1 == INT_MIN && op2 == -1)  {
      assert(0 && "Division: integer overflow!");
    }
  #endif
}
