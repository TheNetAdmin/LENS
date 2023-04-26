#include <stdio.h>

__attribute__((optimize("align-functions=4096")))
void nvleak_print_info(void)
{
	printf("This is nvleak example shared lib\n");
}

__attribute__((optimize("align-functions=4096")))
void nvleak_secret_func_1(void)
{
	// printf("This is nvleak secret func 1\n");
	int tmp = 123;
}

__attribute__((optimize("align-functions=4096")))
void nvleak_secret_func_2(void)
{
	// printf("This is nvleak secret func 2\n");
	int tmp = 456;
}
