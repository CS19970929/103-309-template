#include "main.h"
#include "Runtime.h"

int main(void)
{
		Runtime_Boot();

	while (1)
	{
		Runtime_RunOnce();
	}
}
