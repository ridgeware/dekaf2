#include <mysql.h>

bool test ()
{
	return CLIENT_ZSTD_COMPRESSION;
}

int main()
{
	return test();
}

