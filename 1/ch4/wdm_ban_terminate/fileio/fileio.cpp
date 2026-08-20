#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include "..\common\namespace.h"
#include "..\common\common.h"
#include "..\common\iface.h"

#pragma comment(lib,"user32.lib")

int main()
{
	HANDLE hDevice = (HANDLE)INVALID_HANDLE_VALUE;
	BOOL bRet = FALSE;
	DWORD dwRet = 0;
	DWORD ProcessID;

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

	ProcessID = GetCurrentProcessId();

	DeviceIoControl(
		hDevice,
		IOCTL_SIMPLE_SET_PROCESSID,
		&ProcessID,
		sizeof(DWORD),
		NULL,
		0,
		&dwRet,
		NULL
	);

	MessageBox(0, L"PLZ terminate ME!", L"EXAMPLE", MB_OK);

exit:
	if (hDevice != (HANDLE)INVALID_HANDLE_VALUE)
		CloseHandle(hDevice);

	return 0;
}
