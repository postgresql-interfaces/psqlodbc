/*
 * Test connection settings parsing.
 */
#include <stdio.h>

#include "common.h"

int
main(void)
{
	printf("Testing client_encoding extraction at end of ConnSettings...\n");
	test_connect_ext("ConnSettings=set+client_encoding+to+UTF8");
	test_disconnect();

	return 0;
}
