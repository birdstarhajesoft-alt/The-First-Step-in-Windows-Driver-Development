#include <windows.h>
#include <stdio.h>
#include "..\common\namespace.h"
#include "..\common\common.h"

#define IOCTL_MYSAMPLE_CODE     CTL_CODE( FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS )

#pragma comment(lib, "user32.lib")

int main()
{
	HANDLE hDevice = (HANDLE)INVALID_HANDLE_VALUE;
	BOOL bRet = FALSE;
	DWORD dwRet = 0;
	unsigned char Buffer[5 + 1] = "HELLO";

	hDevice = CreateFile(WIN32_NAME,
		GENERIC_READ | GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		0,
		NULL);
	if (hDevice == (HANDLE)INVALID_HANDLE_VALUE)
		goto exit;

	bRet = DeviceIoControl(
		hDevice,
		IOCTL_MYSAMPLE_CODE,
		NULL,
		0,
		NULL,
		0,
		&dwRet,
		NULL
	);

	printf("write data = %s\n", Buffer);

	bRet = WriteFile(
		hDevice,
		Buffer,
		5,
		&dwRet,
		NULL
	);

	memset(Buffer, 0, 5);

	bRet = ReadFile(
		hDevice,
		Buffer,
		5,
		&dwRet,
		NULL
	);

	printf("read data = %s\n", Buffer);

	MessageBox(NULL, L"Press any key", L"CallExam", MB_OK);

exit:
	if (hDevice != (HANDLE)INVALID_HANDLE_VALUE)
		CloseHandle(hDevice);

	return 0;
}
