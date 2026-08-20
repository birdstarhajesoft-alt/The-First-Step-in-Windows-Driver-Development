#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include "..\common\namespace.h"
#include "..\common\common.h"

int main()
{
	HANDLE hDevice = (HANDLE)INVALID_HANDLE_VALUE;
	BOOL bRet = FALSE;
	DWORD dwRet = 0;
	unsigned char OutputMessage[MESSAGE_SIZE];

	hDevice = CreateFile(WIN32_NAME,
		GENERIC_READ | GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		0,
		NULL);
	if (hDevice == (HANDLE)INVALID_HANDLE_VALUE)
	{
		printf("CreateFile Error!!(%d)\n", GetLastError());
		_getch();
		goto exit;
	}

	printf("Message waiting(5 second)...\n");
	bRet = ReadFile(hDevice, OutputMessage, MESSAGE_SIZE, &dwRet, NULL);
	if( bRet == FALSE )
	{
		printf("ReadFile Error!!(%d)\n", GetLastError());
		_getch();
		goto exit;
	}

	printf("Received message from driver : %s\n", OutputMessage);

exit:
	if (hDevice != (HANDLE)INVALID_HANDLE_VALUE)
		CloseHandle(hDevice);

	return 0;
}
