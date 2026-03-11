#include <sys/random.h>

int main()
{
	char buffer[100];

	if (getrandom(buffer, 100, 0) == 100)
	{
		return 0;
	}

	return 1;
}
