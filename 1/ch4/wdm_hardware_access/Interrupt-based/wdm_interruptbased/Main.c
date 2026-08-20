#include <ntddk.h>
#include "..\common\namespace.h"
#include "..\common\common.h"
#include "hardware.h"

CONTROLLER g_Controller;

typedef struct _DEVICE_EXTENSION
{
    ULONG_PTR	    ullPhysical_ControllerRegisterBase;
    ULONG		    ulPhysical_ControllerRegisterSize;
    ULONG		    ulInterruptVector;
    KIRQL		    Irql;
    void*           pRegisterBase;
    REG_CONTROL*    pRegControl;
    REG_STATUS*     pRegStatus;
    unsigned char*  pTxData;
    unsigned char*  pRxData;
    PKINTERRUPT     pInterruptObject;
    PIRP            pPendingIrp;
}DEVICE_EXTENSION;

// Win32 API CreateFile() ->
NTSTATUS MyCreateDispatch(PDEVICE_OBJECT pDeviceObject, PIRP pIrp)
{
    NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;

    UNREFERENCED_PARAMETER(pDeviceObject);

    DbgPrint("MyCreateDispatch\n");

    ntStatus = STATUS_SUCCESS;
    pIrp->IoStatus.Status = ntStatus;
    IoCompleteRequest(pIrp, 0); // 0 -> IO_NO_INCREMENT
    return ntStatus;
}

// Win32 API CloseHandle() ->
NTSTATUS MyCloseDispatch(PDEVICE_OBJECT pDeviceObject, PIRP pIrp)
{
    NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;

    UNREFERENCED_PARAMETER(pDeviceObject);

    DbgPrint("MyCloseDispatch\n");

    ntStatus = STATUS_SUCCESS;
    pIrp->IoStatus.Status = ntStatus;
	IoCompleteRequest(pIrp, 0); // 0 -> IO_NO_INCREMENT
	return ntStatus;
}

// Win32 API DeviceIoControl() ->
// IOCTL code
#define IOCTL_MYSAMPLE_CODE     CTL_CODE( FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS )

NTSTATUS MyDeviceIoControlDispatch(PDEVICE_OBJECT pDeviceObject, PIRP pIrp)
{
    NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
    DEVICE_EXTENSION* pDeviceExtension = NULL;
    PIO_STACK_LOCATION pStack = NULL;
    ULONG_PTR Information = 0;

    DbgPrint("MyDeviceIoControlDispatch\n");

    UNREFERENCED_PARAMETER(pDeviceObject);

    pDeviceExtension = (DEVICE_EXTENSION*)pDeviceObject->DeviceExtension;

    pStack = IoGetCurrentIrpStackLocation(pIrp);
    switch (pStack->Parameters.DeviceIoControl.IoControlCode)
    {
    case IOCTL_MYSAMPLE_CODE:
        ntStatus = STATUS_SUCCESS;
        Information = 0; 
        break;
    }

    pIrp->IoStatus.Status = ntStatus;
    pIrp->IoStatus.Information = Information;
    IoCompleteRequest(pIrp, 0); // 0 -> IO_NO_INCREMENT
    return ntStatus;
}

// Win32 API WriteFile() ->
NTSTATUS MyWriteDispatch(PDEVICE_OBJECT pDeviceObject, PIRP pIrp)
{
    ULONG dwData;
    REG_CONTROL* pRegControlData = NULL;
    NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
    DEVICE_EXTENSION* pDeviceExtension = NULL;
    PIO_STACK_LOCATION pStack = NULL;
    ULONG Length = 0;
    void* pBuffer = NULL;
    ULONG_PTR Information = 0;

    DbgPrint("MyWriteDispatch\n");

    pDeviceExtension = (DEVICE_EXTENSION*)pDeviceObject->DeviceExtension;

    pStack = IoGetCurrentIrpStackLocation(pIrp);
    Length = pStack->Parameters.Write.Length;
    pBuffer = pIrp->AssociatedIrp.SystemBuffer;

    /////////////////////////////////////////
    // Virtual hardware Tx job

    Length = (Length > HW_TX_BUFFER_SIZE) ? HW_TX_BUFFER_SIZE : Length;

    WRITE_REGISTER_BUFFER_UCHAR(
        pDeviceExtension->pTxData, 
        pBuffer,
        Length
    );

    dwData = READ_REGISTER_ULONG((ULONG*)pDeviceExtension->pRegControl);
    pRegControlData = (REG_CONTROL*)&dwData;
    pRegControlData->DeviceClr = 0;
    pRegControlData->WriteSize = (ULONG)Length;
    pRegControlData->WriteTrigger = 1;
    WRITE_REGISTER_ULONG((ULONG*)pDeviceExtension->pRegControl, dwData);

    ntStatus = STATUS_SUCCESS;
    Information = Length;

    pIrp->IoStatus.Status = ntStatus;
    pIrp->IoStatus.Information = Information;
    IoCompleteRequest(pIrp, 0); // 0 -> IO_NO_INCREMENT
    return ntStatus;
}

// Win32 API ReadFile() ->
NTSTATUS MyReadDispatch(PDEVICE_OBJECT pDeviceObject, PIRP pIrp)
{
    NTSTATUS ntStatus = STATUS_INVALID_PARAMETER;
    DEVICE_EXTENSION* pDeviceExtension = NULL;
    PIO_STACK_LOCATION pStack = NULL;
    ULONG_PTR Information = 0;

    DbgPrint("MyReadDispatch\n");

    // check Parameter
    pStack = IoGetCurrentIrpStackLocation(pIrp);
    if(pStack->Parameters.Read.Length == 0)
        goto exit;

    pDeviceExtension = (DEVICE_EXTENSION*)pDeviceObject->DeviceExtension;
    if (pDeviceExtension->pPendingIrp) // Already pending..
        goto exit;

    pDeviceExtension->pPendingIrp = pIrp; // for ISR->DPC 

    // return Pending
    IoMarkIrpPending(pIrp);

    ntStatus = STATUS_PENDING;
    return ntStatus;

exit:
    pIrp->IoStatus.Status = ntStatus;
    pIrp->IoStatus.Information = Information;
    IoCompleteRequest(pIrp, 0); // 0 -> IO_NO_INCREMENT
    return ntStatus;
}

/////////////////////////////////////////
/// Interrupt service routine
//
BOOLEAN
MyISR(
    __in struct _KINTERRUPT* Interrupt,
    __in PVOID  ServiceContext
)
{
    PDEVICE_OBJECT pDeviceObject = NULL;
    ULONG dwData;
    BOOLEAN bIsOurISR = FALSE;
    REG_STATUS* pRegStatusData = NULL;
    DEVICE_EXTENSION* pDeviceExtension = NULL;

    UNREFERENCED_PARAMETER(Interrupt);

    pDeviceObject = (PDEVICE_OBJECT)ServiceContext;
    pDeviceExtension = (DEVICE_EXTENSION*)pDeviceObject->DeviceExtension;

    // Check whether the interrupt occurred from my hardware
    dwData = READ_REGISTER_ULONG((ULONG*)pDeviceExtension->pRegStatus);
    pRegStatusData = (REG_STATUS  *)&dwData;
    if (pRegStatusData->DeviceAckIRQ)
    {
		pRegStatusData->DeviceAckIRQ = 0;
		WRITE_REGISTER_ULONG((ULONG*)pDeviceExtension->pRegStatus, dwData); // Clear IRQ

        // Add DPC to Queue
        if(pDeviceExtension->pPendingIrp)
            IoRequestDpc(pDeviceObject, pDeviceExtension->pPendingIrp, pDeviceObject);

        bIsOurISR = TRUE;
    }

    return bIsOurISR;
}

/////////////////////////////////////////
/// DPC routine
//
void
MyDPC(
    __in PKDPC  Dpc,
    __in struct _DEVICE_OBJECT* DeviceObject,
    __in struct _IRP* pIrp,
    __in_opt PVOID  Context
)
{
    ULONG dwData;
    DEVICE_EXTENSION* pDeviceExtension = NULL;
    ULONG_PTR Information = 0;
    REG_STATUS* pRegStatusData = NULL;
    REG_CONTROL* pRegControlData = NULL;
    PIO_STACK_LOCATION pStack = NULL;
    ULONG Length = 0;
    void* pBuffer = NULL;

    UNREFERENCED_PARAMETER(Dpc);
    UNREFERENCED_PARAMETER(Context);

    pDeviceExtension = (DEVICE_EXTENSION*)DeviceObject->DeviceExtension;
    
    if (pIrp == NULL)
        goto exit;

    if(pIrp != pDeviceExtension->pPendingIrp)
        goto exit;

    pDeviceExtension->pPendingIrp = NULL;

    pStack = IoGetCurrentIrpStackLocation(pIrp);
    Length = pStack->Parameters.Read.Length;
    pBuffer = pIrp->AssociatedIrp.SystemBuffer;

    dwData = READ_REGISTER_ULONG((ULONG*)pDeviceExtension->pRegStatus);
    pRegStatusData = (REG_STATUS*)&dwData;

    Information = (Length > pRegStatusData->ReadSize) ? pRegStatusData->ReadSize : Length;

    /////////////////////////////////////////
    // Virtual hardware rx job
    READ_REGISTER_BUFFER_UCHAR(
        pDeviceExtension->pRxData,
        pBuffer,
        (ULONG)Information
    );

    dwData = READ_REGISTER_ULONG((ULONG*)pDeviceExtension->pRegControl);
    pRegControlData = (REG_CONTROL*)&dwData;
    pRegControlData->DeviceClr = 1;
    WRITE_REGISTER_ULONG((ULONG*)pDeviceExtension->pRegControl, dwData);

    pIrp->IoStatus.Status = STATUS_SUCCESS;
    pIrp->IoStatus.Information = Information;
    IoCompleteRequest(pIrp, IO_NO_INCREMENT + 2); // Thread priority boosting

exit:
    return;
}


/////////////////////////////////////////
/// Driver unloading
//
void MyDriverUnload(PDRIVER_OBJECT pDriverObject)
{
	UNICODE_STRING SymbolicLinkName;
    PDEVICE_OBJECT pDeviceObject = NULL;
    DEVICE_EXTENSION* pDeviceExtension = NULL;
    ULONG dwData;
    REG_CONTROL* pRegControlData = NULL;

    DbgPrint("MyDriverUnload\n");

    pDeviceObject = pDriverObject->DeviceObject;
    pDeviceExtension = (DEVICE_EXTENSION*)pDeviceObject->DeviceExtension;

    /////////////////////////////////////////
    // Virtual hardware stops operating
    dwData = READ_REGISTER_ULONG((ULONG *)pDeviceExtension->pRegControl);
    pRegControlData = (REG_CONTROL *)&dwData;
    pRegControlData->DeviceEnable = 0;
    WRITE_REGISTER_ULONG((ULONG*)pDeviceExtension->pRegControl, dwData);

    /////////////////////////////////////////
    // ISR uninstall
    IoDisconnectInterrupt(pDeviceExtension->pInterruptObject);

    /////////////////////////////////////////
    // Unmapping virtual address (linear address)
    MmUnmapIoSpace(pDeviceExtension->pRegisterBase, pDeviceExtension->ulPhysical_ControllerRegisterSize);

    /////////////////////////////////////////
    /// Removal of virtual hardware of Non-PnP driver
    //
    hardware_Termination(&g_Controller);

    RtlInitUnicodeString(&SymbolicLinkName, NT_NAME_GLOBAL);
	IoDeleteSymbolicLink(
		&SymbolicLinkName
	);

	IoDeleteDevice(pDriverObject->DeviceObject);
}

/////////////////////////////////////////
/// Driver loading
//
NTSTATUS DriverEntry(PDRIVER_OBJECT pDriverObject, PUNICODE_STRING pRegPath)
{
	BOOLEAN bRet = FALSE;
    UNICODE_STRING DeviceName;
    UNICODE_STRING SymbolicLinkName;
    PDEVICE_OBJECT pDeviceObject = NULL;
    NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
    DEVICE_EXTENSION * pDeviceExtension = NULL;
    PHYSICAL_ADDRESS PhysAddress;
    ULONG dwData;
    REG_CONTROL *pRegControlData = NULL;

    DbgPrint("DriverEntry\n");

    UNREFERENCED_PARAMETER(pRegPath);

    /////////////////////////////////////////
    /// Register driver's callbacks
    //
    pDriverObject->DriverUnload = MyDriverUnload;
    pDriverObject->MajorFunction[IRP_MJ_CREATE] = MyCreateDispatch;
    pDriverObject->MajorFunction[IRP_MJ_READ] = MyReadDispatch;
    pDriverObject->MajorFunction[IRP_MJ_WRITE] = MyWriteDispatch;
    pDriverObject->MajorFunction[IRP_MJ_CLOSE] = MyCloseDispatch;

    /////////////////////////////////////////
    /// create _DEVICE_OBJECT 
    //
    RtlInitUnicodeString(&DeviceName, NT_NAME_DEVICE);
    ntStatus = IoCreateDevice(
        pDriverObject,
        sizeof(DEVICE_EXTENSION),
        &DeviceName,
        FILE_DEVICE_UNKNOWN,
        0,
        FALSE,
        &pDeviceObject
    );
    if (!NT_SUCCESS(ntStatus))
        goto exit;

    pDeviceObject->Flags |= DO_BUFFERED_IO;

    pDeviceExtension = (DEVICE_EXTENSION*)pDeviceObject->DeviceExtension;
    memset(pDeviceExtension, 0, sizeof(DEVICE_EXTENSION));


    // create symbolic link for Win32 API CreateFile()
    RtlInitUnicodeString(&SymbolicLinkName, NT_NAME_GLOBAL);
    ntStatus = IoCreateSymbolicLink(
        &SymbolicLinkName,
        &DeviceName
    );
    if (!NT_SUCCESS(ntStatus))
    {
        IoDeleteDevice(pDeviceObject);
        goto exit;
    }

	/////////////////////////////////////////
	/// Initialization of virtual hardware of Non-PnP driver
    //
	bRet = hardware_Initialize(&g_Controller);

    if (bRet == FALSE)
    {
        IoDeleteSymbolicLink(
            &SymbolicLinkName
        );
        IoDeleteDevice(pDeviceObject);
        return STATUS_UNSUCCESSFUL;
    }

    /////////////////////////////////////////
    /// Get virtual hardware resource information
    //
    pDeviceExtension->ullPhysical_ControllerRegisterBase = 
        g_Controller.ullPhysical_ControllerRegisterBase;
    pDeviceExtension->ulPhysical_ControllerRegisterSize = 
        g_Controller.ulPhysical_ControllerRegisterSize;
    pDeviceExtension->ulInterruptVector = 
        g_Controller.ulInterruptVector;
    pDeviceExtension->Irql = 
        g_Controller.Irql;

    // Virtual address (linear address) mapping for accessing physical memory registers
    PhysAddress.QuadPart = pDeviceExtension->ullPhysical_ControllerRegisterBase;
    pDeviceExtension->pRegisterBase = MmMapIoSpace(
        PhysAddress,
        pDeviceExtension->ulPhysical_ControllerRegisterSize,
        MmNonCached
    );
    pDeviceExtension->pRegControl = (REG_CONTROL *)((unsigned char*)pDeviceExtension->pRegisterBase + HW_REG_CONTROL);
    pDeviceExtension->pRegStatus = (REG_STATUS*)((unsigned char*)pDeviceExtension->pRegisterBase + HW_REG_STATUS);
    pDeviceExtension->pTxData = (unsigned char*)pDeviceExtension->pRegisterBase + HW_REG_BUFFER;
    pDeviceExtension->pRxData = pDeviceExtension->pTxData + HW_TX_BUFFER_SIZE;

    // Virtual hardware DPC object initialization & ISR installation
    IoInitializeDpcRequest(pDeviceObject, MyDPC);

    ntStatus = 
        IoConnectInterrupt(
            &pDeviceExtension->pInterruptObject,
            MyISR,
            pDeviceObject,
            NULL,
            pDeviceExtension->ulInterruptVector,
            pDeviceExtension->Irql,
            pDeviceExtension->Irql,
            LevelSensitive,
            TRUE,
            1,
            FALSE
        );
    if (!NT_SUCCESS(ntStatus))
    {
        MmUnmapIoSpace(pDeviceExtension->pRegisterBase, pDeviceExtension->ulPhysical_ControllerRegisterSize);

        IoDeleteSymbolicLink(
            &SymbolicLinkName
        );
        IoDeleteDevice(pDeviceObject);
        return STATUS_UNSUCCESSFUL;
    }

    // Start virtual hardware operation
    dwData = READ_REGISTER_ULONG((ULONG*)pDeviceExtension->pRegControl);
    pRegControlData = (REG_CONTROL*)&dwData;
    pRegControlData->DeviceEnable = 1;
    pRegControlData->DeviceReqIRQ = 1;
    WRITE_REGISTER_ULONG((ULONG*)pDeviceExtension->pRegControl, dwData);

exit:
	return ntStatus;
}

