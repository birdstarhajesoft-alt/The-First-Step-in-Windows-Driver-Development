#include <ntddk.h>
#include "..\common\namespace.h"
#include "..\common\common.h"

// Arbitrary structure defined by the developer
typedef struct _MY_DEVICE_EXTENSION
{
	KTIMER MyTimer;
	KDPC MyDPC;
}MY_DEVICE_EXTENSION, *PMY_DEVICE_EXTENSION;

VOID
MyDriverUnload(
	PDRIVER_OBJECT pDriverObject
)
{
	UNICODE_STRING GlobalDeviceName;

	RtlInitUnicodeString(&GlobalDeviceName, NT_NAME_GLOBAL);

	// Delete Symbolic Link(Delete Global Name)
	IoDeleteSymbolicLink(&GlobalDeviceName);
	IoDeleteDevice(pDriverObject->DeviceObject);
}

VOID
MyTimerRoutine(
	struct _KDPC* Dpc,
	PVOID  DeferredContext,
	PVOID  SystemArgument1,
	PVOID  SystemArgument2
)
{
	PIRP pIrp = NULL;
	PIO_STACK_LOCATION pStack = NULL;
	char* pBuffer = NULL;
	ULONG ReturnLength = 0;

	UNREFERENCED_PARAMETER(Dpc);
	UNREFERENCED_PARAMETER(SystemArgument1);
	UNREFERENCED_PARAMETER(SystemArgument2);

	pIrp = (PIRP)DeferredContext; // (2) pIrp <---- Pended
	pStack = IoGetCurrentIrpStackLocation(pIrp);

	pBuffer = (char* )pIrp->AssociatedIrp.SystemBuffer; // System buffer for ReadFile()'s user buffer

	strcpy(pBuffer, DRIVER_MESSAGE);
	ReturnLength = (ULONG)sizeof(DRIVER_MESSAGE);

	pIrp->IoStatus.Status = STATUS_SUCCESS;
	pIrp->IoStatus.Information = (ULONG_PTR)ReturnLength; // retLength
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
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

NTSTATUS MyDriverReadDispatchRoutine(
	PDEVICE_OBJECT pDeviceObject,
	PIRP pIrp
)
{
	NTSTATUS ntStatus = STATUS_INVALID_PARAMETER;;
	ULONG RequestLength = 0;
	ULONG ReturnLength = 0;
	LARGE_INTEGER Timout;
	PMY_DEVICE_EXTENSION pMyDeviceExtension = NULL;
	void* pBuffer = pIrp->AssociatedIrp.SystemBuffer; // System buffer for ReadFile()'s user buffer
	PIO_STACK_LOCATION pStack = IoGetCurrentIrpStackLocation(pIrp);

	// Parameter checking
	RequestLength = pStack->Parameters.Read.Length; // ReadFile()'s Length
	if (!pBuffer || (RequestLength != MESSAGE_SIZE))
		goto exit;

	pMyDeviceExtension = (PMY_DEVICE_EXTENSION)pDeviceObject->DeviceExtension;

	// Initialize Timer
	KeInitializeTimer(&pMyDeviceExtension->MyTimer);

	// Initialize DPC
	KeInitializeDpc(&pMyDeviceExtension->MyDPC, MyTimerRoutine, pIrp); // (1) pIrp <--- DPC Context. (1) -> (2)

	// Pending
	IoMarkIrpPending(pIrp);

	// Set timeout
	Timout.QuadPart = TIMEOUT_5S * -1; // Relative
	KeSetTimer(&pMyDeviceExtension->MyTimer, Timout, &pMyDeviceExtension->MyDPC); // Oneshot time out

	return STATUS_PENDING;

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

	UNREFERENCED_PARAMETER(pRegPath);
	pDriverObject->DriverUnload = MyDriverUnload;
	pDriverObject->MajorFunction[IRP_MJ_CREATE] = MyDriverCreateDispatchRoutine;
	pDriverObject->MajorFunction[IRP_MJ_READ] = MyDriverReadDispatchRoutine;

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
	pDeviceObject->Flags |= DO_BUFFERED_IO; // Buffer strategy for IRP_MJ_READ/IRP_MJ_WRITE

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