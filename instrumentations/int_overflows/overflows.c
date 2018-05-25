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
        assert(0 && "Multiplication: integer underflow!");
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

void __INSTR_check_add_long(long x, long y) {
      if((x > 0) && (y > 0) && (x > LONG_MAX - y)) {
	      assert(0 && "Addition: integer overflow!");
      }

      if((x < 0) && (y < 0) && (x < LONG_MIN - y)) {
	      assert(0 && "Addition: integer underflow!");
      }
}

void __INSTR_check_sub_long(long x, long y) {
      if((y > 0) && (x < LONG_MIN +  y)) {
	      assert(0 && "Substraction: integer underflow!");
      }

      if((y < 0) && (x > LONG_MAX + y)) {
	      assert(0 && "Substraction: integer overflow!");
      }
}

void __INSTR_check_mul_long(long x, long y) {
    if (x > LONG_MAX / y) {
	    assert(0 && "Multiplication: integer overflow!");
    }
    if ((x < LONG_MIN / y)) {
        assert(0 && "Multiplication: integer underflow!");
    }

    // 2's complement detection
    #if (LONG_MIN != -LONG_MAX)
      if ((x == -1) && (y == LONG_MIN))
      if ((y == -1) && (x == LONG_MIN))
    #endif
}

int __INSTR_check_div_long(long op1, long op2) {
  if (op2 == 0) {
    assert(0 && "Division by zero!");
  }
  // 2's complement detection
  #if (LONG_MIN != -LONG_MAX)
    if (op1 == LONG_MIN && op2 == -1)  {
      assert(0 && "Division: integer overflow!");
    }
  #endif
}

void __INSTR_check_add_short(short x, short y) {
      if((x > 0) && (y > 0) && (x > SHRT_MAX - y)) {
	      assert(0 && "Addition: integer overflow!");
      }

      if((x < 0) && (y < 0) && (x < SHRT_MIN - y)) {
	      assert(0 && "Addition: integer underflow!");
      }
}

void __INSTR_check_sub_short(short x, short y) {
      if((y > 0) && (x < SHRT_MIN +  y)) {
	      assert(0 && "Substraction: integer underflow!");
      }

      if((y < 0) && (x > SHRT_MAX + y)) {
	      assert(0 && "Substraction: integer overflow!");
      }
}

void __INSTR_check_mul_short(short x, short y) {
    if (x > SHRT_MAX / y) {
	    assert(0 && "Multiplication: integer overflow!");
    }
    if ((x < SHRT_MIN / y)) {
        assert(0 && "Multiplication: integer underflow!");
    }

    // 2's complement detection
    #if (SHRT_MIN != -SHRT_MAX)
      if ((x == -1) && (y == SHRT_MIN))
      if ((y == -1) && (x == SHRT_MIN))
    #endif
}

int __INSTR_check_div_short(short op1, short op2) {
  if (op2 == 0) {
    assert(0 && "Division by zero!");
  }
  // 2's complement detection
  #if (SHRT_MIN != -SHRT_MAX)
    if (op1 == SHRT_MIN && op2 == -1)  {
      assert(0 && "Division: integer overflow!");
    }
  #endif
}

void __INSTR_check_add_char(char x, char y) {
      if((x > 0) && (y > 0) && (x > SCHAR_MAX - y)) {
	      assert(0 && "Addition: char overflow!");
      }

      if((x < 0) && (y < 0) && (x < SCHAR_MIN - y)) {
	      assert(0 && "Addition: char underflow!");
      }
}

void __INSTR_check_sub_char(char x, char y) {
      if((y > 0) && (x < SCHAR_MIN +  y)) {
	      assert(0 && "Substraction: char underflow!");
      }

      if((y < 0) && (x > SCHAR_MAX + y)) {
	      assert(0 && "Substraction: char overflow!");
      }
}

void __INSTR_check_mul_char(char x, char y) {
    if (x > SCHAR_MAX / y) {
	    assert(0 && "Multiplication: char overflow!");
    }
    if ((x < SCHAR_MIN / y)) {
        assert(0 && "Multiplication: char underflow!");
    }

    // 2's complement detection
    #if (SCHAR_MIN != -SCHAR_MAX)
      if ((x == -1) && (y == SCHAR_MIN))
      if ((y == -1) && (x == SCHAR_MIN))
    #endif
}

int __INSTR_check_div_char(char op1, char op2) {
  if (op2 == 0) {
    assert(0 && "Division by zero!");
  }
  // 2's complement detection
  #if (SCHAR_MIN != -SCHAR_MAX)
    if (op1 == SCHAR_MIN && op2 == -1)  {
      assert(0 && "Division: char overflow!");
    }
  #endif
}
