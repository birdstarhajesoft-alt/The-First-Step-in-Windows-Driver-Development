#include <fltKernel.h>
#include <dontuse.h>
#include <suppress.h>
#include "..\common\namespace.h"
#include "..\common\common.h"

// Arbitrary structure defined by the developer
typedef struct _MY_DEVICE_EXTENSION
{
	PFLT_FILTER FilterHandle;
}MY_DEVICE_EXTENSION, *PMY_DEVICE_EXTENSION;

NTSTATUS
PtInstanceSetup(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_SETUP_FLAGS Flags,
	_In_ DEVICE_TYPE VolumeDeviceType,
	_In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType
)
{
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(Flags);
	UNREFERENCED_PARAMETER(VolumeDeviceType);
	UNREFERENCED_PARAMETER(VolumeFilesystemType);

	PAGED_CODE();

	return STATUS_SUCCESS;
}

NTSTATUS
PtInstanceQueryTeardown(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
)
{
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(Flags);

	PAGED_CODE();

	return STATUS_SUCCESS;
}

VOID
PtInstanceTeardownStart(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
)
{
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(Flags);

	PAGED_CODE();
}


VOID
PtInstanceTeardownComplete(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
)
{
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(Flags);

	PAGED_CODE();
}

FLT_POSTOP_CALLBACK_STATUS
PtPostOperationPassThrough(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_opt_ PVOID CompletionContext,
	_In_ FLT_POST_OPERATION_FLAGS Flags
)
{
	UNREFERENCED_PARAMETER(Data);
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(CompletionContext);
	UNREFERENCED_PARAMETER(Flags);

	return FLT_POSTOP_FINISHED_PROCESSING;
}

FLT_PREOP_CALLBACK_STATUS
PtPreOperation_IRP_MJ_CREATE(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID* CompletionContext
)
{
	FLT_PREOP_CALLBACK_STATUS returnStatus = FLT_PREOP_SUCCESS_WITH_CALLBACK;
	WCHAR* pFilenamePartialPathName = NULL;

	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(CompletionContext);

	// You need to check whether the file has a specific extension.
	// To do this, inspect the FileObject
	if (FltObjects->FileObject != NULL)
	{
		pFilenamePartialPathName = (WCHAR*)ExAllocatePoolWithTag(NonPagedPool, FltObjects->FileObject->FileName.Length + 2, 'HJHJ');
		if (pFilenamePartialPathName == NULL)
			goto exit;

		memset(pFilenamePartialPathName, 0, FltObjects->FileObject->FileName.Length + 2);
		memcpy(pFilenamePartialPathName, FltObjects->FileObject->FileName.Buffer, FltObjects->FileObject->FileName.Length);
		_wcsupr(pFilenamePartialPathName);
		if (wcswcs(pFilenamePartialPathName, L".AAA"))
		{
			// If the file has a specific extension,
			Data->IoStatus.Status = STATUS_ACCESS_DENIED; // It is considered that an error occurred due to a specific reason
			returnStatus = FLT_PREOP_COMPLETE; // Complete the corresponding command
			goto exit;
		}
	}

exit:
	if (pFilenamePartialPathName)
		ExFreePool(pFilenamePartialPathName);

	return returnStatus;
}

CONST FLT_OPERATION_REGISTRATION Callbacks[] = {
	{ IRP_MJ_CREATE,
	  0,
	  PtPreOperation_IRP_MJ_CREATE, 
	  PtPostOperationPassThrough },
	{ IRP_MJ_OPERATION_END }
};

CONST FLT_REGISTRATION FilterRegistration = {
	sizeof(FLT_REGISTRATION),         //  Size
	FLT_REGISTRATION_VERSION,           //  Version
	0,                                  //  Flags
	NULL,                               //  Context
	Callbacks,                          //  Operation callbacks
	NULL,	                            //  MiniFilterUnload
	PtInstanceSetup,                    //  InstanceSetup
	PtInstanceQueryTeardown,            //  InstanceQueryTeardown
	PtInstanceTeardownStart,            //  InstanceTeardownStart
	PtInstanceTeardownComplete,         //  InstanceTeardownComplete
	NULL,                               //  GenerateFileName
	NULL,                               //  GenerateDestinationFileName
	NULL                                //  NormalizeNameComponent

};

VOID
MyDriverUnload(
	PDRIVER_OBJECT pDriverObject
)
{
	UNICODE_STRING GlobalDeviceName;
	PMY_DEVICE_EXTENSION pDeviceExtension = NULL;
	PDEVICE_OBJECT pDeviceObject = pDriverObject->DeviceObject;

	pDeviceExtension = (PMY_DEVICE_EXTENSION)pDeviceObject->DeviceExtension;

	FltUnregisterFilter(pDeviceExtension->FilterHandle);

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

	ntStatus = FltRegisterFilter(pDriverObject,
		&FilterRegistration,
		&pMyDeviceExtension->FilterHandle);

	if (!NT_SUCCESS(ntStatus))
	{
		goto exit;
	}

	//
	//  Start filtering i/o
	//

	ntStatus = FltStartFiltering(pMyDeviceExtension->FilterHandle);

	if (!NT_SUCCESS(ntStatus)) {
		FltUnregisterFilter(pMyDeviceExtension->FilterHandle);
		goto exit;
	}

	// Create Symbolic Link(Make Global Name)
	ntStatus = IoCreateSymbolicLink(&GlobalDeviceName, &UniDeviceName);
	if (!NT_SUCCESS(ntStatus))
	{
		FltUnregisterFilter(pMyDeviceExtension->FilterHandle);
		goto exit;
	}

exit:
	if (!NT_SUCCESS(ntStatus) && pDriverObject->DeviceObject)
	{
		IoDeleteDevice(pDriverObject->DeviceObject);
	}

	return ntStatus;
}