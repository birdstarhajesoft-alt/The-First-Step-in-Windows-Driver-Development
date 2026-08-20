#include <ntddk.h>
#include "..\common\namespace.h"
#include "..\common\common.h"
#include "..\common\iface.h"

#define PROCESS_TERMINATE       0x0001	// TerminatEProcess

extern "C" 
NTSTATUS
PsLookupProcessByProcessId(
	IN HANDLE ProcessId,
	OUT PEPROCESS* Process
);

// Arbitrary structure defined by the developer
typedef struct _MY_DEVICE_EXTENSION
{
	void* ObjectCallbackHandle = NULL;
	PEPROCESS	Target_EProcess;
}MY_DEVICE_EXTENSION, *PMY_DEVICE_EXTENSION;


OB_PREOP_CALLBACK_STATUS
PreOperationCallback(
	__in PVOID  RegistrationContext,
	__in POB_PRE_OPERATION_INFORMATION  OperationInformation
)
{
	PMY_DEVICE_EXTENSION pDeviceExtension = NULL;

	pDeviceExtension = (PMY_DEVICE_EXTENSION)RegistrationContext;

	if (pDeviceExtension->Target_EProcess != OperationInformation->Object) // _EProcess
		goto exit;

	if ((OperationInformation->Operation == OB_OPERATION_HANDLE_CREATE))
	{
		if ((OperationInformation->Parameters->CreateHandleInformation.OriginalDesiredAccess & PROCESS_TERMINATE)
			== PROCESS_TERMINATE) // If you try to open a process with exit privileges,
		{
			OperationInformation->Parameters->CreateHandleInformation.DesiredAccess &= ~PROCESS_TERMINATE;
			// Proceed with permissions removed
		}
	}

exit:
	return OB_PREOP_SUCCESS;
}

VOID
PostOperationCallback(
	__in PVOID  RegistrationContext,
	__in POB_POST_OPERATION_INFORMATION  OperationInformation
)
{
	UNREFERENCED_PARAMETER(RegistrationContext);
	UNREFERENCED_PARAMETER(OperationInformation);
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

	if(pDeviceExtension->ObjectCallbackHandle)
		ObUnRegisterCallbacks(pDeviceExtension->ObjectCallbackHandle);

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
	ULONG* phProcessID = NULL; // from User application
	PIO_STACK_LOCATION pStack = IoGetCurrentIrpStackLocation(pIrp);
	ULONG InBufferLength, IOCTLcode;
	ULONG ReturnLength = 0;

	pMyDeviceExtension = (PMY_DEVICE_EXTENSION)pDeviceObject->DeviceExtension;

	InBufferLength = pStack->Parameters.DeviceIoControl.InputBufferLength;// DeviceIoControl()'s InputBufferLength
	IOCTLcode = pStack->Parameters.DeviceIoControl.IoControlCode;// DeviceIoControl()'s IOCTL code

	switch (pStack->Parameters.DeviceIoControl.IoControlCode)
	{
	case IOCTL_SIMPLE_SET_PROCESSID:
		// check parameter
		if (InBufferLength != sizeof(ULONG))
			goto exit;
		phProcessID = (ULONG*)pIrp->AssociatedIrp.SystemBuffer;// DeviceIoControl()'s InputBuffer

		ntStatus = PsLookupProcessByProcessId(
			(HANDLE)(*phProcessID),
			&pMyDeviceExtension->Target_EProcess
		);
		break;
	}

exit:
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

	obRegistration.Version = ObGetFilterVersion();
	obRegistration.OperationRegistrationCount = 1;
	RtlInitUnicodeString(&obRegistration.Altitude, L"300000");
	obRegistration.RegistrationContext = pMyDeviceExtension;
	opRegistration.ObjectType = PsProcessType;
	opRegistration.Operations = OB_OPERATION_HANDLE_CREATE;	// Create/Open
	opRegistration.PreOperation = PreOperationCallback;		// register PreOperation
	opRegistration.PostOperation = PostOperationCallback;	// register PostOperation
	obRegistration.OperationRegistration = &opRegistration;

	ntStatus = ObRegisterCallbacks(&obRegistration, &pMyDeviceExtension->ObjectCallbackHandle);
	if (!NT_SUCCESS(ntStatus))
	{
		goto exit;
	}

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