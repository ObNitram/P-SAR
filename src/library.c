#include "library.h"

#include <stdio.h>

#include "utils/utils.h"

void hello(void)
{
	printf("Hello, World!\n");

	int a = 1;
	int b = 3;

	int c = add(a, b);

	printf("%i + %i = %i", a, b, c);
}