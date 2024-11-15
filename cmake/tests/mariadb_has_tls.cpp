#include <mysql.h>

bool test ()
{
	return MYSQL_OPT_SSL_ENFORCE;
}

int main()
{
	return test();
}

