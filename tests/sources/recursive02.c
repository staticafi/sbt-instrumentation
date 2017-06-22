#include<stdlib.h>

void foo(int count){
	if(count >= 0) {
		void (*foo_pointer)(int);
		foo_pointer = &foo;
		foo_pointer(--count);
	}
}

void call_foo(){
	foo(10);
}

int main(void){

	void (*call_foo_pointer)();
	call_foo_pointer = &call_foo;

	(*call_foo_pointer)();

	return 0;
}
