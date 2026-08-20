#include <ntddk.h>
#include "..\common\namespace.h"

#define MAX_DEVICE_COUNT	(1)	// for Only one device

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
	UNICODE_STRING GlobalDeviceName;

	RtlInitUnicodeString(&GlobalDeviceName, NT_NAME_GLOBAL);

	// Delete Symbolic Link(Delete Global Name)
	IoDeleteSymbolicLink(&GlobalDeviceName);
	internal_DeleteAllDeviceObject(pDriverObject);
}

NTSTATUS MyDriverCreateDispatchRoutine(
	PDEVICE_OBJECT pDeviceObject,
	PIRP pIrp
)
{
	UNREFERENCED_PARAMETER(pDeviceObject);

	pIrp->IoStatus.Status = STATUS_SUCCESS;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

extern "C" NTSTATUS DriverEntry(
	PDRIVER_OBJECT pDriverObject,
	PUNICODE_STRING pRegPath
)
{
	int times = 0;
	UNICODE_STRING UniDeviceName;
	UNICODE_STRING GlobalDeviceName;
	NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
	PDEVICE_OBJECT pDeviceObject = NULL;
	PMY_DEVICE_EXTENSION pMyDeviceExtension = NULL;

	UNREFERENCED_PARAMETER(pRegPath);
	pDriverObject->DriverUnload = MyDriverUnload;
	pDriverObject->MajorFunction[IRP_MJ_CREATE] = MyDriverCreateDispatchRoutine;

	RtlInitUnicodeString(&UniDeviceName, NT_NAME_DEVICE);
	RtlInitUnicodeString(&GlobalDeviceName, NT_NAME_GLOBAL);

	for (times = 0; times < MAX_DEVICE_COUNT; times++)
	{
		// create _DEVICE_OBJECT   
		ntStatus = IoCreateDevice(
			pDriverObject,
			sizeof(MY_DEVICE_EXTENSION), // Arbitrary structure defined by the developer
			&UniDeviceName,
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

		// Create Symbolic Link(Make Global Name)
		ntStatus = IoCreateSymbolicLink(&GlobalDeviceName, &UniDeviceName);
		if (!NT_SUCCESS(ntStatus))
		{
			goto exit;
		}
	}

exit:
	if (!NT_SUCCESS(ntStatus) && pDriverObject->DeviceObject)
	{
		internal_DeleteAllDeviceObject(pDriverObject);
	}

	return ntStatus;
}