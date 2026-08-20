#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include "..\common\namespace.h"

#include <winioctl.h>
#include "..\common\iface.h"

int main()
{
	HANDLE hDevice = (HANDLE)INVALID_HANDLE_VALUE;
	BOOL bRet = FALSE;
	DWORD dwRet = 0;
	unsigned char InputBuffer[1024];
	unsigned char OutputBuffer[512];

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

	memset(InputBuffer, '1', sizeof(InputBuffer));
	WriteFile(hDevice, InputBuffer, sizeof(InputBuffer), &dwRet, NULL);

	ReadFile(hDevice, OutputBuffer, sizeof(OutputBuffer), &dwRet, NULL);

	DeviceIoControl(
		hDevice,
		IOCTL_SIMPLE_METHOD_BUFFERED,
		InputBuffer,
		sizeof(InputBuffer),
		OutputBuffer,
		sizeof(OutputBuffer),
		&dwRet, NULL
	);

	DeviceIoControl(
		hDevice,
		IOCTL_SIMPLE_METHOD_IN_DIRECT,
		InputBuffer,
		sizeof(InputBuffer),
		OutputBuffer,
		sizeof(OutputBuffer),
		&dwRet, NULL
	);

	DeviceIoControl(
		hDevice,
		IOCTL_SIMPLE_METHOD_OUT_DIRECT,
		InputBuffer,
		sizeof(InputBuffer),
		OutputBuffer,
		sizeof(OutputBuffer),
		&dwRet, NULL
	);

	DeviceIoControl(
		hDevice,
		IOCTL_SIMPLE_METHOD_NEITHER,
		InputBuffer,
		sizeof(InputBuffer),
		OutputBuffer,
		sizeof(OutputBuffer),
		&dwRet, NULL
	);

exit:
	if (hDevice != (HANDLE)INVALID_HANDLE_VALUE)
		CloseHandle(hDevice);

	return 0;
}
