#include <assert.h>
#include <stdint.h>

void __INSTR_check_add_i32(int32_t x, int32_t y) {
      if((x > 0) && (y > 0) && (x > (INT32_MAX - y))) {
	      assert(0 && "Addition: integer overflow!");
      }

      if((x < 0) && (y < 0) && (x < (INT32_MIN - y))) {
	      assert(0 && "Addition: integer underflow!");
      }
}

void __INSTR_check_sub_i32(int32_t x, int32_t y) {
      if((y > 0) && (x < (INT32_MIN +  y))) {
	      assert(0 && "Substraction: integer underflow!");
      }

      if((y < 0) && (x > (INT32_MAX + y))) {
	      assert(0 && "Substraction: integer overflow!");
      }
}

void __INSTR_check_mul_i32(int32_t x, int32_t y) {
   // if (x == 0 || y == 0)
   //     return;

    int64_t result = (int64_t)x * (int64_t)y;

    if (result > INT32_MAX)
	    assert(0 && "Multiplication: integer overflow!");

    if (result < INT32_MIN)
	    assert(0 && "Multiplication: integer underflow!");

    /*if (x > INT32_MAX / y) {
	    assert(0 && "Multiplication: integer overflow!");
    }
    if (x < INT32_MIN / y) {
        assert(0 && "Multiplication: integer underflow!");
    }

    if ((x == -1) && (y == INT32_MIN))
        assert(0 && "Multiplication: integer overflow!");
    if ((y == -1) && (x == INT32_MIN))
        assert(0 && "Multiplication: integer overflow!");*/
}

void __INSTR_check_div_i32(int32_t op1, int32_t op2) {
  if (op2 == 0) {
    assert(0 && "Division by zero!");
  }

  if (op1 == INT32_MIN && op2 == -1)  {
    assert(0 && "Division: integer overflow!");
  }
}

void __INSTR_check_add_i64(int64_t x, int64_t y) {
      if((x > 0) && (y > 0) && (x > (INT64_MAX - y))) {
	      assert(0 && "Addition: integer overflow!");
      }

      if((x < 0) && (y < 0) && (x < (INT64_MIN - y))) {
	      assert(0 && "Addition: integer underflow!");
      }
}

void __INSTR_check_sub_i64(int64_t x, int64_t y) {
      if((y > 0) && (x < (INT64_MIN + y))) {
	      assert(0 && "Substraction: integer underflow!");
      }

      if((y < 0) && (x > (INT64_MAX + y))) {
	      assert(0 && "Substraction: integer overflow!");
      }
}

void __INSTR_check_mul_i64(int64_t x, int64_t y) {
    if (x == 0 || y == 0)
        return;

    if (y > 0 && x > (INT64_MAX / y)) {
	    assert(0 && "Multiplication: integer overflow!");
    }
    if (y < 0 && x > (INT64_MIN / y)) {
        assert(0 && "Multiplication: integer underflow!");
    }

    if ((x == -1) && (y == INT64_MIN))
	  assert(0 && "Multiplication: integer overflow!");
    if ((y == -1) && (x == INT64_MIN))
	  assert(0 && "Multiplication: integer overflow!");
}

void __INSTR_check_div_i64(int64_t op1, int64_t op2) {
  if (op2 == 0) {
    assert(0 && "Division by zero!");
  }

  if (op1 == INT64_MIN && op2 == -1)  {
    assert(0 && "Division: integer overflow!");
  }
}

void __INSTR_check_add_i16(int16_t x, int16_t y) {
      if((x > 0) && (y > 0) && (x > (INT16_MAX - y))) {
	      assert(0 && "Addition: integer overflow!");
      }

      if((x < 0) && (y < 0) && (x < (INT16_MIN - y))) {
	      assert(0 && "Addition: integer underflow!");
      }
}

void __INSTR_check_sub_i16(int16_t x, int16_t y) {
      if((y > 0) && (x < (INT16_MIN + y))) {
	      assert(0 && "Substraction: integer underflow!");
      }

      if((y < 0) && (x > (INT16_MAX + y))) {
	      assert(0 && "Substraction: integer overflow!");
      }
}

void __INSTR_check_mul_i16(int16_t x, int16_t y) {
    if (x == 0 || y == 0)
        return;

    if (x > (INT16_MAX / y)) {
	    assert(0 && "Multiplication: integer overflow!");
    }
    if (x < (INT16_MIN / y)) {
        assert(0 && "Multiplication: integer underflow!");
    }

    if ((x == -1) && (y == INT16_MIN))
	    assert(0 && "Multiplication: integer overflow!");
    if ((y == -1) && (x == INT16_MIN))
	    assert(0 && "Multiplication: integer overflow!");
}

void __INSTR_check_div_i16(int16_t op1, int16_t op2) {
  if (op2 == 0) {
    assert(0 && "Division by zero!");
  }

  if (op1 == INT16_MIN && op2 == -1)  {
    assert(0 && "Division: integer overflow!");
  }
}

void __INSTR_check_add_i8(int8_t x, int8_t y) {
      if((x > 0) && (y > 0) && (x > (INT8_MAX - y))) {
	      assert(0 && "Addition: int8_t overflow!");
      }

      if((x < 0) && (y < 0) && (x < (INT8_MIN - y))) {
	      assert(0 && "Addition: int8_t underflow!");
      }
}

void __INSTR_check_sub_i8(int8_t x, int8_t y) {
      if((y > 0) && (x < (INT8_MIN + y))) {
	      assert(0 && "Substraction: int8_t underflow!");
      }

      if((y < 0) && (x > (INT8_MAX + y))) {
	      assert(0 && "Substraction: int8_t overflow!");
      }
}

void __INSTR_check_mul_i8(int8_t x, int8_t y) {
    if (x == 0 || y == 0)
        return;

    if (x > (INT8_MAX / y)) {
	    assert(0 && "Multiplication: int8_t overflow!");
    }
    if ((x < (INT8_MIN / y))) {
        assert(0 && "Multiplication: int8_t underflow!");
    }

    if ((x == -1) && (y == INT8_MIN))
	    assert(0 && "Multiplication: int8_t overflow!");
    if ((y == -1) && (x == INT8_MIN))
	    assert(0 && "Multiplication: int8_t overflow!");
}

void __INSTR_check_div_i8(int8_t op1, int8_t op2) {
  if (op2 == 0) {
    assert(0 && "Division by zero!");
  }

  if (op1 == INT8_MIN && op2 == -1)  {
    assert(0 && "Division: int8_t overflow!");
  }
}

void __INSTR_check_shl_i32(int32_t a, uint32_t e) {
  if (e == 0)
      return;

  if (e >= 32) {
      assert(0 && "Shift by >= sizeof(int)");
  }

  if (a > 0) {
	assert((a <= (INT32_MAX >> e)) && "Invalid shift");
  } else if (a < 0) {
	assert((a >= (INT32_MIN >> e)) && "Invalid shift");
  }
}

void __INSTR_check_shl_i64(int64_t a, uint64_t e) {
  if (e == 0)
      return;

  if (e >= 64) {
      assert(0 && "Shift by >= sizeof(int)");
  }

  if (a > 0) {
	assert((a <= (INT64_MAX >> e)) && "Invalid shift");
  } else if (a < 0) {
	assert((a >= (INT64_MIN >> e)) && "Invalid shift");
  }
}
