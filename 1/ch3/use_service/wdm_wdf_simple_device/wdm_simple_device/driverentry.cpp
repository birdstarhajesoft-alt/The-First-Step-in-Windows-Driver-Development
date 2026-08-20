#include <ntddk.h>

#define MAX_DEVICE_COUNT	(2)

// Arbitrary structure defined by the developer
typedef struct _MY_DEVICE_EXTENSION
{
	int times; // example
}MY_DEVICE_EXTENSION, *PMY_DEVICE_EXTENSION;

void internal_DeleteAllDeviceObject(PDRIVER_OBJECT pDriverObject)
{
	while (pDriverObject->DeviceObject)
	{
		IoDeleteDevice(pDriverObject->DeviceObject);
	} // Remove all _DEVICE_OBJECT created by Driver
}

VOID
MyDriverUnload(
	PDRIVER_OBJECT pDriverObject
)
{
	internal_DeleteAllDeviceObject(pDriverObject);
}

extern "C" NTSTATUS DriverEntry(
	PDRIVER_OBJECT pDriverObject,
	PUNICODE_STRING pRegPath
)
{
	int times = 0;
	NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
	PDEVICE_OBJECT pDeviceObject = NULL;
	PMY_DEVICE_EXTENSION pMyDeviceExtension = NULL;

	UNREFERENCED_PARAMETER(pRegPath);
	pDriverObject->DriverUnload = MyDriverUnload;

	for (times = 0; times < MAX_DEVICE_COUNT; times++)
	{
		// create _DEVICE_OBJECT   
		ntStatus = IoCreateDevice(
			pDriverObject,
			sizeof(MY_DEVICE_EXTENSION), // Arbitrary structure defined by the developer
			NULL,
			FILE_DEVICE_UNKNOWN, // Arbitrary _DEVICE_OBJECT type
			0,
			FALSE,
			&pDeviceObject);

		if (!NT_SUCCESS(ntStatus))
		{
			goto exit;
		}

		pMyDeviceExtension = (PMY_DEVICE_EXTENSION)pDeviceObject->DeviceExtension;
		pMyDeviceExtension->times = times; // use
	}

exit:
	if (!NT_SUCCESS(ntStatus) && pDriverObject->DeviceObject)
	{
		internal_DeleteAllDeviceObject(pDriverObject);
	}

	return ntStatus;
}