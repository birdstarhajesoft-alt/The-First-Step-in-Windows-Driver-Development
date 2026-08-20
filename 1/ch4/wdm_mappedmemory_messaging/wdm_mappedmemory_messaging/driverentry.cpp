#include <ntddk.h>
#include "..\common\namespace.h"
#include "..\common\common.h"
#include "..\common\iface.h"

// Arbitrary structure defined by the developer
typedef struct _MY_DEVICE_EXTENSION
{
	BOOLEAN bTimerOn;
	KTIMER	MyTimer;
	KDPC	MyDPC;

	PVOID	pEventObject;	// from Applicaiton hEvent
	PMDL	pMdl;			// for mapping
	PVOID   pMappedAddress;	// for mapped
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
	PMY_DEVICE_EXTENSION pMyDeviceExtension = NULL;
	char* pBuffer = NULL;

	UNREFERENCED_PARAMETER(Dpc);
	UNREFERENCED_PARAMETER(SystemArgument1);
	UNREFERENCED_PARAMETER(SystemArgument2);

	pMyDeviceExtension = (PMY_DEVICE_EXTENSION)DeferredContext; // (2) 

	if (pMyDeviceExtension->bTimerOn == FALSE) // check
		goto exit;

	pMyDeviceExtension->bTimerOn = FALSE;

	if (pMyDeviceExtension->pMdl == NULL)
		goto exit;

	pBuffer = (char *)pMyDeviceExtension->pMappedAddress;
	if(pBuffer == NULL)
		goto exit;

	strcpy(pBuffer, DRIVER_MESSAGE);
	if (pMyDeviceExtension->pEventObject) 
	{
		KeSetEvent((PKEVENT)pMyDeviceExtension->pEventObject, 0, FALSE);
	}
exit:
	return;
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

NTSTATUS MyDriverCloseDispatchRoutine(
	PDEVICE_OBJECT pDeviceObject,
	PIRP pIrp
)
{
	PMY_DEVICE_EXTENSION pMyDeviceExtension = NULL;
	pMyDeviceExtension = (PMY_DEVICE_EXTENSION)pDeviceObject->DeviceExtension;

	if (pMyDeviceExtension->bTimerOn == TRUE)
	{
		pMyDeviceExtension->bTimerOn = FALSE;
		KeCancelTimer(&pMyDeviceExtension->MyTimer);
	}

	if (pMyDeviceExtension->pMdl) // Already mappped(for implied)
	{
		pMyDeviceExtension->pMappedAddress = NULL;
		MmUnlockPages(pMyDeviceExtension->pMdl);
		IoFreeMdl(pMyDeviceExtension->pMdl);
		pMyDeviceExtension->pMdl = NULL;
	}

	if (pMyDeviceExtension->pEventObject) // Already event prepared(for implied)
	{
		ObDereferenceObject(pMyDeviceExtension->pEventObject);
		pMyDeviceExtension->pEventObject = NULL;
	}

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
	IOCTL_SIMPLE_MAP_MEMORY_PARAMS* pMapMemoryParams = NULL; // from User application
	HANDLE *phEvent = NULL; // from User application
	PIO_STACK_LOCATION pStack = IoGetCurrentIrpStackLocation(pIrp);
	ULONG InBufferLength, IOCTLcode;
	ULONG ReturnLength = 0;
	LARGE_INTEGER Timout;

	pMyDeviceExtension = (PMY_DEVICE_EXTENSION)pDeviceObject->DeviceExtension;

	InBufferLength = pStack->Parameters.DeviceIoControl.InputBufferLength;// DeviceIoControl()'s InputBufferLength
	IOCTLcode = pStack->Parameters.DeviceIoControl.IoControlCode;// DeviceIoControl()'s IOCTL code

	switch (pStack->Parameters.DeviceIoControl.IoControlCode)
	{
	case IOCTL_SIMPLE_SET_EVENTHANDLE:
		// check handle prepared
		if (pMyDeviceExtension->pEventObject)// already
		{
			ntStatus = STATUS_INVALID_DEVICE_REQUEST;
			goto exit;
		}
		// check parameter
		if (InBufferLength != sizeof(HANDLE))
			goto exit;
		phEvent = (HANDLE*)pIrp->AssociatedIrp.SystemBuffer;// DeviceIoControl()'s InputBuffer

		ntStatus = ObReferenceObjectByHandle(
			*phEvent,
			0,
			*ExEventObjectType,
			KernelMode,
			&pMyDeviceExtension->pEventObject,
			NULL);
		if (!NT_SUCCESS(ntStatus))
			goto exit;

		ntStatus = STATUS_SUCCESS;
		break;
	case IOCTL_SIMPLE_DELETE_EVENTHANDLE:
		// check handle prepared
		if (pMyDeviceExtension->pEventObject == NULL)// not yet
		{
			ntStatus = STATUS_INVALID_DEVICE_REQUEST;
			goto exit;
		}
		ObDereferenceObject(pMyDeviceExtension->pEventObject);
		pMyDeviceExtension->pEventObject = NULL;

		ntStatus = STATUS_SUCCESS;
		break;
	case IOCTL_SIMPLE_MAP_MEMORY:
		// check memory mapped
		if (pMyDeviceExtension->pMdl)// already
		{
			ntStatus = STATUS_INVALID_DEVICE_REQUEST;
			goto exit;
		}
		// check parameter
		if (InBufferLength != sizeof(IOCTL_SIMPLE_MAP_MEMORY_PARAMS))
			goto exit;
		pMapMemoryParams = (IOCTL_SIMPLE_MAP_MEMORY_PARAMS *)pIrp->AssociatedIrp.SystemBuffer;
		// DeviceIoControl()'s InputBuffer

		// make MDL
		pMyDeviceExtension->pMdl = IoAllocateMdl(
			pMapMemoryParams->pUserBuffer,
			(ULONG)pMapMemoryParams->BufferSize,
			FALSE,
			FALSE,
			NULL
		);
		if (pMyDeviceExtension->pMdl == NULL)// error
		{
			goto exit;
		}

		// lock pages
		MmProbeAndLockPages(pMyDeviceExtension->pMdl, UserMode, IoWriteAccess);
		pMyDeviceExtension->pMappedAddress = (char*)MmGetSystemAddressForMdlSafe(
			pMyDeviceExtension->pMdl, HighPagePriority);
		if (pMyDeviceExtension->pMappedAddress == NULL)// error
		{
			IoFreeMdl(pMyDeviceExtension->pMdl);
			pMyDeviceExtension->pMdl = NULL;
			goto exit;
		}

		ntStatus = STATUS_SUCCESS;
		break;
	case IOCTL_SIMPLE_UNMAP_MEMORY:
		// check memory mapped
		if (pMyDeviceExtension->pMdl == NULL)// not yet
		{
			ntStatus = STATUS_INVALID_DEVICE_REQUEST;
			goto exit;
		}

		pMyDeviceExtension->pMappedAddress = NULL;
		MmUnlockPages(pMyDeviceExtension->pMdl);
		IoFreeMdl(pMyDeviceExtension->pMdl);
		pMyDeviceExtension->pMdl = NULL;

		ntStatus = STATUS_SUCCESS;
		break;
	case IOCTL_SIMPLE_DO_SETTIMER:
		// check timer ready
		if (pMyDeviceExtension->bTimerOn == TRUE)
		{
			pMyDeviceExtension->bTimerOn = FALSE;
			KeCancelTimer(&pMyDeviceExtension->MyTimer);
		}
		Timout.QuadPart = TIMEOUT_5S * -1; // Relative
		KeSetTimer(&pMyDeviceExtension->MyTimer, Timout, &pMyDeviceExtension->MyDPC); // Oneshot time out
		pMyDeviceExtension->bTimerOn = TRUE;
		ntStatus = STATUS_SUCCESS;
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

	UNREFERENCED_PARAMETER(pRegPath);
	pDriverObject->DriverUnload = MyDriverUnload;
	pDriverObject->MajorFunction[IRP_MJ_CREATE] = MyDriverCreateDispatchRoutine;
	pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = MyDriverDeviceIoControlDispatchRoutine;
	pDriverObject->MajorFunction[IRP_MJ_CLOSE] = MyDriverCloseDispatchRoutine;

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

	memset(pMyDeviceExtension, 0, sizeof(MY_DEVICE_EXTENSION));

	// Initialize Timer
	KeInitializeTimer(&pMyDeviceExtension->MyTimer);

	// Initialize DPC
	KeInitializeDpc(&pMyDeviceExtension->MyDPC, MyTimerRoutine, pMyDeviceExtension);
	// (1) pMyDeviceExtension <--- DPC Context. (1) -> (2)

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