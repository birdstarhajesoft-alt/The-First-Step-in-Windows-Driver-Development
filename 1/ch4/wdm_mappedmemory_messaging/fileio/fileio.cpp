#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include "..\common\namespace.h"
#include "..\common\common.h"
#include "..\common\iface.h"

int main()
{
	HANDLE hDevice = (HANDLE)INVALID_HANDLE_VALUE;
	BOOL bRet = FALSE;
	DWORD dwRet = 0;
	HANDLE hEvent = NULL;
	IOCTL_SIMPLE_MAP_MEMORY_PARAMS	MapMemoryParams = { 0, };
	unsigned char MessageBuffer[MESSAGE_SIZE];

	hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (hEvent == NULL)
	{
		printf("CreateEvent Error!!(%d)\n", GetLastError());
		_getch();
		goto exit;
	}

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

	DeviceIoControl(
		hDevice, 
		IOCTL_SIMPLE_SET_EVENTHANDLE,
		&hEvent,
		sizeof(HANDLE),
		NULL,
		0,
		&dwRet, 
		NULL
	);

	MapMemoryParams.pUserBuffer = MessageBuffer;
	MapMemoryParams.BufferSize = MESSAGE_SIZE;

	DeviceIoControl(
		hDevice,
		IOCTL_SIMPLE_MAP_MEMORY,
		&MapMemoryParams,
		sizeof(IOCTL_SIMPLE_MAP_MEMORY_PARAMS),
		NULL,
		0,
		&dwRet,
		NULL
	);

	DeviceIoControl(
		hDevice,
		IOCTL_SIMPLE_DO_SETTIMER,
		NULL,
		0,
		NULL,
		0,
		&dwRet,
		NULL
	);

	printf("Waiting...\n");
	dwRet = WaitForSingleObject(hEvent, INFINITE);
	if (dwRet == 0) // signaled
	{
		printf("Received message from driver : %s\n", MessageBuffer);

		DeviceIoControl(
			hDevice,
			IOCTL_SIMPLE_UNMAP_MEMORY,
			NULL,
			0,
			NULL,
			0,
			&dwRet,
			NULL
		);

		DeviceIoControl(
			hDevice,
			IOCTL_SIMPLE_DELETE_EVENTHANDLE,
			NULL,
			0,
			NULL,
			0,
			&dwRet,
			NULL
		);
		_getch();
	}

exit:
	if(hEvent)
		CloseHandle(hEvent);

	if (hDevice != (HANDLE)INVALID_HANDLE_VALUE)
		CloseHandle(hDevice);

	return 0;
}
