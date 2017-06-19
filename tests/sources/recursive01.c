void recursive(int i){
	if(i <= 0){
		return;
	}

	int a[10];
	a[0] = 1;
	a[100] = 2;
	recursive(--i);
}

int main(void){
	recursive(10);
	return 0;
}
