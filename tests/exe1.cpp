#include <stdio.h>


void lib2();


int main()
{
	printf("exe: %d\n", __LINE__);
	lib2();
	return 0;
}
