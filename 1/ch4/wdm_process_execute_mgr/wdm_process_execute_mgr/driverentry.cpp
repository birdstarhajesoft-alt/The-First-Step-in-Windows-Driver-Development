#include <ntddk.h>
#include <stdio.h>
#include "..\common\namespace.h"
#include "..\common\common.h"

extern "C" 
NTSTATUS
PsLookupProcessByProcessId(
	IN HANDLE ProcessId,
	OUT PEPROCESS* Process
);

extern "C"
PFILE_OBJECT
PsGetProcessImageFileName(
	IN HANDLE ProcessId
);

// Arbitrary structure defined by the developer
typedef struct _MY_DEVICE_EXTENSION
{
	void* ObjectCallbackHandle = NULL;
	PEPROCESS	Target_EProcess;
}MY_DEVICE_EXTENSION, *PMY_DEVICE_EXTENSION;

void
ConvertUnicodeString2WCharString(WCHAR* pSrcString, SIZE_T Bytes, WCHAR* pDesString)
{
	memset(pDesString, 0, Bytes);
	memcpy(pDesString, pSrcString, Bytes - 2);
}

void doCreateProcess(PEPROCESS  Process, HANDLE  ProcessID, PPS_CREATE_NOTIFY_INFO  CreateInfo)
{
	WCHAR* pFullPathString = NULL;
	HANDLE  ParentID = 0;
	SIZE_T size = 0;
	NTSTATUS ntStatus;
	PDEVICE_OBJECT pVolumeDevObj = NULL;
	WCHAR* pSymbolString = NULL;
	UNICODE_STRING* pUniString = NULL;
	PFILE_OBJECT pParentFileObject = NULL;

	UNREFERENCED_PARAMETER(Process);
	UNREFERENCED_PARAMETER(ProcessID);

	if (CreateInfo == NULL)
		goto exit;

	if (CreateInfo->ImageFileName == NULL)
		goto exit;

	ParentID = CreateInfo->ParentProcessId;

	pParentFileObject = PsGetProcessImageFileName(ParentID); // Not using...

#if 0
	UNREFERENCED_PARAMETER(pVolumeDevObj);
	UNREFERENCED_PARAMETER(ntStatus);	
	UNREFERENCED_PARAMETER(pUniString);

	// Using method : ImageFileName
	size = (SIZE_T)CreateInfo->ImageFileName->Length;
	size += 1024; // for free space
	pFullPathString = (WCHAR*)ExAllocatePoolWithTag(NonPagedPool, size, 'HJHJ');
	if (pFullPathString == NULL)
		goto exit;
	memset(pFullPathString, 0, size);

	// CreateInfo->ImageFileName->Buffer = "\\??\\C:\\..."
	memcpy(pFullPathString, CreateInfo->ImageFileName->Buffer + 4, CreateInfo->ImageFileName->Length - 4);
	_wcsupr(pFullPathString);
#endif

#if 1
	// Using method : FileObject
	size = (SIZE_T)CreateInfo->FileObject->FileName.Length;
	size += 1024; // for free space
	pFullPathString = (WCHAR*)ExAllocatePoolWithTag(NonPagedPool, size, 'HJHJ');
	if (pFullPathString == NULL)
		goto exit;
	memset(pFullPathString, 0, size);

	pSymbolString = (WCHAR*)ExAllocatePoolWithTag(NonPagedPool, 1024, 'HJHJ');
	if (pSymbolString == NULL)
		goto exit;
	pUniString = (UNICODE_STRING*)pSymbolString;
	pVolumeDevObj = CreateInfo->FileObject->DeviceObject;
	ntStatus = IoVolumeDeviceToDosName(pVolumeDevObj, pUniString); // "\\Device\\HarddiskVolume1" -> "C:"
	if (!NT_SUCCESS(ntStatus))
		goto exit;
	memcpy(pFullPathString, pUniString->Buffer, pUniString->Length);
	memcpy(pFullPathString + pUniString->Length/sizeof(WCHAR), CreateInfo->FileObject->FileName.Buffer, CreateInfo->FileObject->FileName.Length);
	_wcsupr(pFullPathString);
#endif

	// If the conditions are met, the action is prohibited (execution of programs running in a folder named BAN is prohibited)
	if (wcswcs(pFullPathString, L"\\BAN\\"))
	{
		CreateInfo->CreationStatus = STATUS_UNSUCCESSFUL;
	}

exit:
	if (pSymbolString)
		ExFreePool(pSymbolString);

	if (pFullPathString)
		ExFreePool(pFullPathString);

	return;
}

VOID
NotifyRoutine(
	__inout PEPROCESS  Process,
	__in HANDLE  ProcessId,
	__in_opt PPS_CREATE_NOTIFY_INFO  CreateInfo
)
{
	if (CreateInfo)
	{
		doCreateProcess(Process, ProcessId, CreateInfo);
	}

	return;
}

VOID
MyDriverUnload(
	PDRIVER_OBJECT pDriverObject
)
{
	UNICODE_STRING GlobalDeviceName;
	PMY_DEVICE_EXTENSION pDeviceExtension = NULL;
	PDEVICE_OBJECT pDeviceObject = pDriverObject->DeviceObject;

	pDeviceExtension = (PMY_DEVICE_EXTENSION)pDeviceObject->DeviceExtension;

	PsSetCreateProcessNotifyRoutineEx(NotifyRoutine, TRUE);

	RtlInitUnicodeString(&GlobalDeviceName, NT_NAME_GLOBAL);

	// Delete Symbolic Link(Delete Global Name)
	IoDeleteSymbolicLink(&GlobalDeviceName);
	IoDeleteDevice(pDriverObject->DeviceObject);
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

	ntStatus = PsSetCreateProcessNotifyRoutineEx(NotifyRoutine, FALSE);
	if (!NT_SUCCESS(ntStatus))
		goto exit;

	// Create Symbolic Link(Make Global Name)
	ntStatus = IoCreateSymbolicLink(&GlobalDeviceName, &UniDeviceName);
	if (!NT_SUCCESS(ntStatus))
	{
		goto exit;
	}

exit:
	if (!NT_SUCCESS(ntStatus) && pDriverObject->DeviceObject)
	{
		IoDeleteDevice(pDriverObject->DeviceObject);
	}

	return ntStatus;
}