#include <assert.h>
#include <stdint.h>
#include <math.h>

void __INSTR_check_add_i32(int32_t x, int32_t y) {
      if((x > 0) && (y > 0) && (x > (pow(2, 31) - 1) - y)) {
	      assert(0 && "Addition: integer overflow!");
      }

      if((x < 0) && (y < 0) && (x < (-pow(2, 31)) - y)) {
	      assert(0 && "Addition: integer underflow!");
      }
}

void __INSTR_check_sub_i32(int32_t x, int32_t y) {
      if((y > 0) && (x < (-pow(2, 31)) +  y)) {
	      assert(0 && "Substraction: integer underflow!");
      }

      if((y < 0) && (x > (pow(2, 31) - 1) + y)) {
	      assert(0 && "Substraction: integer overflow!");
      }
}

void __INSTR_check_mul_i32(int32_t x, int32_t y) {
    if (x > (pow(2, 31) - 1) / y) {
	    assert(0 && "Multiplication: integer overflow!");
    }
    if ((x < (-pow(2, 31)) / y)) {
        assert(0 && "Multiplication: integer underflow!");
    }

    if ((x == -1) && (y == (-pow(2, 31))))
        assert(0 && "Multiplication: integer overflow!");
    if ((y == -1) && (x == (-pow(2, 31))))
        assert(0 && "Multiplication: integer overflow!");
}

void __INSTR_check_div_i32(int32_t op1, int32_t op2) {
  if (op2 == 0) {
    assert(0 && "Division by zero!");
  }

  if (op1 == (-pow(2, 31)) && op2 == -1)  {
    assert(0 && "Division: integer overflow!");
  }
}

void __INSTR_check_add_i64(int64_t x, int64_t y) {
      if((x > 0) && (y > 0) && (x > (pow(2, 63) - 1) - y)) {
	      assert(0 && "Addition: integer overflow!");
      }

      if((x < 0) && (y < 0) && (x < (-pow(2, 63)) - y)) {
	      assert(0 && "Addition: integer underflow!");
      }
}

void __INSTR_check_sub_i64(int64_t x, int64_t y) {
      if((y > 0) && (x < (-pow(2, 63)) +  y)) {
	      assert(0 && "Substraction: integer underflow!");
      }

      if((y < 0) && (x > (pow(2, 63) - 1) + y)) {
	      assert(0 && "Substraction: integer overflow!");
      }
}

void __INSTR_check_mul_i64(int64_t x, int64_t y) {
    if (x > (pow(2, 63) - 1) / y) {
	    assert(0 && "Multiplication: integer overflow!");
    }
    if ((x < (-pow(2, 63)) / y)) {
        assert(0 && "Multiplication: integer underflow!");
    }

    if ((x == -1) && (y == (-pow(2, 63))))
	  assert(0 && "Multiplication: integer overflow!");
    if ((y == -1) && (x == (-pow(2, 63))))
	  assert(0 && "Multiplication: integer overflow!");
}

void __INSTR_check_div_i64(int64_t op1, int64_t op2) {
  if (op2 == 0) {
    assert(0 && "Division by zero!");
  }

  if (op1 == (-pow(2, 63)) && op2 == -1)  {
    assert(0 && "Division: integer overflow!");
  }
}

void __INSTR_check_add_i16(int16_t x, int16_t y) {
      if((x > 0) && (y > 0) && (x > (pow(2, 15) - 1) - y)) {
	      assert(0 && "Addition: integer overflow!");
      }

      if((x < 0) && (y < 0) && (x < (-pow(2, 15)) - y)) {
	      assert(0 && "Addition: integer underflow!");
      }
}

void __INSTR_check_sub_i16(int16_t x, int16_t y) {
      if((y > 0) && (x < (-pow(2, 15)) +  y)) {
	      assert(0 && "Substraction: integer underflow!");
      }

      if((y < 0) && (x > (pow(2, 15) - 1) + y)) {
	      assert(0 && "Substraction: integer overflow!");
      }
}

void __INSTR_check_mul_i16(int16_t x, int16_t y) {
    if (x > (pow(2, 15) - 1) / y) {
	    assert(0 && "Multiplication: integer overflow!");
    }
    if ((x < (-pow(2, 15)) / y)) {
        assert(0 && "Multiplication: integer underflow!");
    }

    if ((x == -1) && (y == (-pow(2, 15))))
	    assert(0 && "Multiplication: integer overflow!");
    if ((y == -1) && (x == (-pow(2, 15))))
	    assert(0 && "Multiplication: integer overflow!");
}

void __INSTR_check_div_i16(int16_t op1, int16_t op2) {
  if (op2 == 0) {
    assert(0 && "Division by zero!");
  }

  if (op1 == (-pow(2, 15)) && op2 == -1)  {
    assert(0 && "Division: integer overflow!");
  }
}

void __INSTR_check_add_i8(int8_t x, int8_t y) {
      if((x > 0) && (y > 0) && (x > (pow(2, 7) - 1) - y)) {
	      assert(0 && "Addition: int8_t overflow!");
      }

      if((x < 0) && (y < 0) && (x < (-pow(2, 7)) - y)) {
	      assert(0 && "Addition: int8_t underflow!");
      }
}

void __INSTR_check_sub_i8(int8_t x, int8_t y) {
      if((y > 0) && (x < (-pow(2, 7)) +  y)) {
	      assert(0 && "Substraction: int8_t underflow!");
      }

      if((y < 0) && (x > (pow(2, 7) - 1) + y)) {
	      assert(0 && "Substraction: int8_t overflow!");
      }
}

void __INSTR_check_mul_i8(int8_t x, int8_t y) {
    if (x > (pow(2, 7) - 1) / y) {
	    assert(0 && "Multiplication: int8_t overflow!");
    }
    if ((x < (-pow(2, 7)) / y)) {
        assert(0 && "Multiplication: int8_t underflow!");
    }

    if ((x == -1) && (y == (-pow(2, 7))))
	    assert(0 && "Multiplication: int8_t overflow!");
    if ((y == -1) && (x == (-pow(2, 7))))
	    assert(0 && "Multiplication: int8_t overflow!");
}

void __INSTR_check_div_i8(int8_t op1, int8_t op2) {
  if (op2 == 0) {
    assert(0 && "Division by zero!");
  }

  if (op1 == (-pow(2, 7)) && op2 == -1)  {
    assert(0 && "Division: int8_t overflow!");
  }
}
