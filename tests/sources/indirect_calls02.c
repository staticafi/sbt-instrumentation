#include<stdlib.h>

void foo2(int);

void foo3(int count){
	if(count >= 0) {
		foo2(count);
	}
}

void foo2(int count){
	foo3(count);
}

void foo1(){
	foo3(10);
}

int main(void){
	foo1();
	return 0;
}
