#include <windows.h>
#include <stdio.h>
#include <conio.h>

#include "..\common\namespace.h"

int main()
{
	HANDLE hDevice = (HANDLE)INVALID_HANDLE_VALUE;
	BOOL bRet = FALSE;
	DWORD dwRet = 0;

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
	else
	{
		printf("CreateFile Success!!\n");
		_getch();
	}

exit:
	if (hDevice != (HANDLE)INVALID_HANDLE_VALUE)
		CloseHandle(hDevice);

	return 0;
}
