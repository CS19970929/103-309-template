#include "main.h"
#include "AppInit.h"
#include "Runtime.h"

int main(void)
{
	AppInit_Boot();

	while (1)
	{
		Runtime_RunOnce();
	}
}
