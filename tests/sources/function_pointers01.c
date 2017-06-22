#include<stdlib.h>

int sum(int count){
	
	int sum = 0;

	for(int i=1; i<=count; i++) {
		sum += i;
	}

	return sum;
}

int main(void){

	int (*sum_pointer)(int);
	sum_pointer = &sum;

	int sum = (*sum_pointer)(5);

	return 0;
}
