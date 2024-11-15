#include <mysql.h>

bool test ()
{
	return MYSQL_OPT_COMPRESSION_ALGORITHMS;
}

int main()
{
	return test();
}

