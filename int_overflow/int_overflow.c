#include <stdlib.h>
#include <assert.h>
#include <limits.h>

void __INSTR_check_addition(int x, int y) {
      // y > 0
      if((y > 0) && (x > INT_MAX - y)) {
	      assert(0);
      }

      // y < 0
      if((y < 0) && (x < INT_MIN - y)) {
	      assert(0);
      }
}

void __INSTR_check_subtraction(int x, int y) {

      // y > 0
      if((y > 0) && (x < INT_MIN +  y)) {
	      assert(0);
      }

      // y < 0
      if((y < 0) && (x > INT_MAX + y)) {
	      assert(0);
      }
}

void __INSTR_check_multiplication(int x, int y) {
	//TODO
}
