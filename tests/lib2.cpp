#include <stdio.h>


void lib1();


void lib2()
{
	printf("lib2: %d\n", __LINE__);
	lib1();
}
