#include <assert.h>
#include <stdint.h>

extern void __VERIFIER_error() __attribute__((noreturn));

void __INSTR_check_add_i32(int32_t x, int32_t y) {
      if((x > 0) && (y > 0) && (x > (INT32_MAX - y))) {
          __VERIFIER_error();
      }

      if((x < 0) && (y < 0) && (x < (INT32_MIN - y))) {
          __VERIFIER_error();
      }
}

void __INSTR_check_sub_i32(int32_t x, int32_t y) {
      if((y > 0) && (x < (INT32_MIN + y))) {
          __VERIFIER_error();
      }

      if((y < 0) && (x > (INT32_MAX + y))) {
          __VERIFIER_error();
      }
}

void __INSTR_check_mul_i32(int32_t x, int32_t y) {
   // if (x == 0 || y == 0)
   //     return;

    int64_t result = (int64_t)x * (int64_t)y;

    if (result > INT32_MAX)
        __VERIFIER_error();

    if (result < INT32_MIN)
        __VERIFIER_error();

    /*if (x > INT32_MAX / y) {
      __VERIFIER_error();
    }
    if (x < INT32_MIN / y) {
    __VERIFIER_error();
    }

    if ((x == -1) && (y == INT32_MIN))
        __VERIFIER_error();
    if ((y == -1) && (x == INT32_MIN))
        __VERIFIER_error();*/
}

void __INSTR_check_div_i32(int32_t op1, int32_t op2) {
  if (op2 == 0) {
      __VERIFIER_error();
  }

  if (op1 == INT32_MIN && op2 == -1)  {
      __VERIFIER_error();
  }
}

void __INSTR_check_add_i64(int64_t x, int64_t y) {
      if((x > 0) && (y > 0) && (x > (INT64_MAX - y))) {
          __VERIFIER_error();
      }

      if((x < 0) && (y < 0) && (x < (INT64_MIN - y))) {
          __VERIFIER_error();
      }
}

void __INSTR_check_sub_i64(int64_t x, int64_t y) {
      if((y > 0) && (x < (INT64_MIN + y))) {
          __VERIFIER_error();
      }

      if((y < 0) && (x > (INT64_MAX + y))) {
          __VERIFIER_error();
      }
}

void __INSTR_check_mul_i64(int64_t x, int64_t y) {
    if (x == 0 || y == 0)
        return;

    if (y > 0 && x > (INT64_MAX / y)) {
        __VERIFIER_error();
    }

    if (y < -1 && x > (INT64_MIN / y)) {
        __VERIFIER_error();
    }

    if (x > 0 && y > (INT64_MAX / x)) {
        __VERIFIER_error();
    }

    if (x < -1 && y > (INT64_MIN / x)) {
        __VERIFIER_error();
    }

    if ((x == -1) && (y == INT64_MIN))
        __VERIFIER_error();
    if ((y == -1) && (x == INT64_MIN))
        __VERIFIER_error();
}

void __INSTR_check_div_i64(int64_t op1, int64_t op2) {
  if (op2 == 0) {
      __VERIFIER_error();
  }

  if (op1 == INT64_MIN && op2 == -1)  {
      __VERIFIER_error();
  }
}

void __INSTR_check_add_i16(int16_t x, int16_t y) {
      if((x > 0) && (y > 0) && (x > (INT16_MAX - y))) {
          __VERIFIER_error();
      }

      if((x < 0) && (y < 0) && (x < (INT16_MIN - y))) {
          __VERIFIER_error();
      }
}

void __INSTR_check_sub_i16(int16_t x, int16_t y) {
      if((y > 0) && (x < (INT16_MIN + y))) {
          __VERIFIER_error();
      }

      if((y < 0) && (x > (INT16_MAX + y))) {
          __VERIFIER_error();
      }
}

void __INSTR_check_mul_i16(int16_t x, int16_t y) {
    if (x == 0 || y == 0)
        return;

    if (x > INT16_MAX / y) {
        __VERIFIER_error();
    }
    if (x < INT16_MIN / y) {
        __VERIFIER_error();
    }

    if ((x == -1) && (y == INT16_MIN))
        __VERIFIER_error();
    if ((y == -1) && (x == INT16_MIN))
        __VERIFIER_error();
}

void __INSTR_check_div_i16(int16_t op1, int16_t op2) {
  if (op2 == 0) {
      __VERIFIER_error();
  }

  if (op1 == INT16_MIN && op2 == -1)  {
      __VERIFIER_error();
  }
}

void __INSTR_check_add_i8(int8_t x, int8_t y) {
      if((x > 0) && (y > 0) && (x > (INT8_MAX - y))) {
          __VERIFIER_error();
      }

      if((x < 0) && (y < 0) && (x < (INT8_MIN - y))) {
          __VERIFIER_error();
      }
}

void __INSTR_check_sub_i8(int8_t x, int8_t y) {
      if((y > 0) && (x < (INT8_MIN + y))) {
          __VERIFIER_error();
      }

      if((y < 0) && (x > (INT8_MAX + y))) {
          __VERIFIER_error();
      }
}

void __INSTR_check_mul_i8(int8_t x, int8_t y) {
    if (x == 0 || y == 0)
        return;

    if (x > (INT8_MAX / y)) {
        __VERIFIER_error();
    }
    if ((x < (INT8_MIN / y))) {
        __VERIFIER_error();
    }

    if ((x == -1) && (y == INT8_MIN))
        __VERIFIER_error();
    if ((y == -1) && (x == INT8_MIN))
        __VERIFIER_error();
}

void __INSTR_check_div_i8(int8_t op1, int8_t op2) {
  if (op2 == 0) {
      __VERIFIER_error();
  }

  if (op1 == INT8_MIN && op2 == -1)  {
      __VERIFIER_error();
  }
}

void __INSTR_check_shl_i32(int32_t a, uint32_t e) {
  if (e == 0)
      return;

  if (e >= 32) {
      __VERIFIER_error();
  }

  if (a > 0) {
      __VERIFIER_error();
  } else if (a < 0) {
      __VERIFIER_error();
  }
}

void __INSTR_check_shl_i64(int64_t a, uint64_t e) {
  if (e == 0)
      return;

  if (e >= 64) {
      __VERIFIER_error();
  }

  if (a > 0) {
      __VERIFIER_error();
  } else if (a < 0) {
      __VERIFIER_error();
  }
}
