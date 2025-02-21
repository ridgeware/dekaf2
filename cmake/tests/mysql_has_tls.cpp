#include <mysql.h>

bool test ()
{
	return MYSQL_OPT_SSL_MODE;
}

int main()
{
	return test();
}

