#include <ntddk.h>
#include <stdio.h>
#include "..\common\namespace.h"
#include "..\common\common.h"

// Arbitrary structure defined by the developer
typedef struct _MY_DEVICE_EXTENSION
{
	LARGE_INTEGER Cookie;
}MY_DEVICE_EXTENSION, *PMY_DEVICE_EXTENSION;

VOID
MyDriverUnload(
	PDRIVER_OBJECT pDriverObject
)
{
	UNICODE_STRING GlobalDeviceName;
	PMY_DEVICE_EXTENSION pDeviceExtension = NULL;
	PDEVICE_OBJECT pDeviceObject = pDriverObject->DeviceObject;

	pDeviceExtension = (PMY_DEVICE_EXTENSION)pDeviceObject->DeviceExtension;

	if(pDeviceExtension->Cookie.QuadPart)
		CmUnRegisterCallback(pDeviceExtension->Cookie);

	RtlInitUnicodeString(&GlobalDeviceName, NT_NAME_GLOBAL);

	// Delete Symbolic Link(Delete Global Name)
	IoDeleteSymbolicLink(&GlobalDeviceName);
	IoDeleteDevice(pDriverObject->DeviceObject);
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

NTSTATUS MyDriverDeviceIoControlDispatchRoutine(
	PDEVICE_OBJECT pDeviceObject,
	PIRP pIrp
)
{
	PMY_DEVICE_EXTENSION pMyDeviceExtension = NULL;
	NTSTATUS ntStatus = STATUS_INVALID_PARAMETER;
	PIO_STACK_LOCATION pStack = IoGetCurrentIrpStackLocation(pIrp);
	ULONG InBufferLength, IOCTLcode;
	ULONG ReturnLength = 0;

	pMyDeviceExtension = (PMY_DEVICE_EXTENSION)pDeviceObject->DeviceExtension;

	InBufferLength = pStack->Parameters.DeviceIoControl.InputBufferLength;// DeviceIoControl()'s InputBufferLength
	IOCTLcode = pStack->Parameters.DeviceIoControl.IoControlCode;// DeviceIoControl()'s IOCTL code

	switch (pStack->Parameters.DeviceIoControl.IoControlCode) // Developer codes
	{
		case 0:// Developer codes
			break;
	}

	pIrp->IoStatus.Status = ntStatus;
	pIrp->IoStatus.Information = (ULONG_PTR)ReturnLength; // retLength
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	return ntStatus;
}

NTSTATUS
RegistryCallback(
	PVOID  CallbackContext,
	PVOID  Argument1,
	PVOID  Argument2
)
{
	PMY_DEVICE_EXTENSION pDeviceExtension = NULL;
	ULONG_PTR Cmd_ptr;
	ULONG Cmd;
	UNICODE_STRING * pRootFullPathString = NULL;
	NTSTATUS returnStatus = STATUS_SUCCESS;
	ULONG_PTR ID;
	WCHAR* pFullPathName = NULL;
	SIZE_T Size = 0;

	Cmd_ptr = (ULONG_PTR)Argument1;
	Cmd = (ULONG)Cmd_ptr;
	pDeviceExtension = (PMY_DEVICE_EXTENSION)CallbackContext;

	switch (Cmd)
	{
		case RegNtPreCreateKeyEx:
		{
			PREG_CREATE_KEY_INFORMATION	pInformation;

			pInformation = (PREG_CREATE_KEY_INFORMATION)Argument2;
			if (!pInformation)
				break;

			if (pInformation->CompleteName->Length)
			{
				Size += pInformation->CompleteName->Length;
			}
			if (pInformation->RootObject) // for Relative path
			{
				returnStatus = CmCallbackGetKeyObjectIDEx(
					&pDeviceExtension->Cookie,
					pInformation->RootObject,
					&ID,
					(PCUNICODE_STRING *)&pRootFullPathString,
					0
				);
				if (NT_SUCCESS(returnStatus))
				{
					Size += pRootFullPathString->Length;
				}
			}
			
			returnStatus = STATUS_SUCCESS;

			if (Size)
			{
				Size += 2; // appending '\\' + NULL
				pFullPathName = (WCHAR*)ExAllocatePoolWithTag(
					NonPagedPool, Size * 2, 'HJHJ');
			}
			if (pFullPathName)
			{
				if (pInformation->RootObject)
				{
					swprintf(pFullPathName, L"%wZ\\%wZ", 
						pRootFullPathString, pInformation->CompleteName);
				}
				else
				{
					swprintf(pFullPathName, L"%wZ", 
						pInformation->CompleteName);
				}

				// BAN
				_wcsupr(pFullPathName);
				if (wcswcs(pFullPathName, L"HACKING"))
				{
					returnStatus = STATUS_ACCESS_DENIED; // No access
				}
			}
			break;
		}
	}
	if (pRootFullPathString)
	{
		CmCallbackReleaseKeyObjectIDEx(pRootFullPathString);
	}
	if (pFullPathName)
	{
		ExFreePool(pFullPathName);
	}
	return returnStatus;
}


extern "C" NTSTATUS DriverEntry(
	PDRIVER_OBJECT pDriverObject,
	PUNICODE_STRING pRegPath
)
{
	UNICODE_STRING UniDeviceName;
	UNICODE_STRING GlobalDeviceName;
	NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
	PDEVICE_OBJECT pDeviceObject = NULL;
	PMY_DEVICE_EXTENSION pMyDeviceExtension = NULL;
	OB_CALLBACK_REGISTRATION obRegistration = { 0, };
	OB_OPERATION_REGISTRATION opRegistration = { 0, };

	UNREFERENCED_PARAMETER(pRegPath);
	pDriverObject->DriverUnload = MyDriverUnload;
	pDriverObject->MajorFunction[IRP_MJ_CREATE] = MyDriverCreateDispatchRoutine;
	pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = MyDriverDeviceIoControlDispatchRoutine;

	RtlInitUnicodeString(&UniDeviceName, NT_NAME_DEVICE);
	RtlInitUnicodeString(&GlobalDeviceName, NT_NAME_GLOBAL);

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

	ntStatus = CmRegisterCallback(RegistryCallback, pMyDeviceExtension, &pMyDeviceExtension->Cookie);

	if (!NT_SUCCESS(ntStatus))
	{
		IoDeleteDevice(pDeviceObject);
		goto exit;
	}

	// Create Symbolic Link(Make Global Name)
	ntStatus = IoCreateSymbolicLink(&GlobalDeviceName, &UniDeviceName);
	if (!NT_SUCCESS(ntStatus))
	{
		goto exit;
	}

exit:
	if (!NT_SUCCESS(ntStatus))
	{
		IoDeleteDevice(pDriverObject->DeviceObject);
	}

	return ntStatus;
}