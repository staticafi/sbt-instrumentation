#include<stdlib.h>

void foo3(int count){
	if(count >= 0) {
		void (*foo_pointer)(int);
		foo_pointer = &foo3;
		foo_pointer(--count);
	}
}

int foo2(){
	return 10;
}

void foo1(){
	foo3(foo2());
}

void call_foo1(){
	foo1();
}

int main(void){

	void (*call_foo_pointer)();
	call_foo_pointer = &call_foo1;

	(*call_foo_pointer)();

	return 0;
}
