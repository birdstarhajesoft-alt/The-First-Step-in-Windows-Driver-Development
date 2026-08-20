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

    // polling
    do
    {
        dwData = READ_REGISTER_ULONG((ULONG*)pDeviceExtension->pRegControl);
        pRegControlData = (REG_CONTROL*)&dwData;        
    } while (pRegControlData->WriteTrigger);

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
    ULONG dwData;
    REG_CONTROL* pRegControlData = NULL;
    REG_STATUS* pRegStatusData = NULL;
    NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
    DEVICE_EXTENSION* pDeviceExtension = NULL;
    PIO_STACK_LOCATION pStack = NULL;
    ULONG Length = 0;
    void* pBuffer = NULL;
    ULONG_PTR Information = 0;

    DbgPrint("MyReadDispatch\n");

    pDeviceExtension = (DEVICE_EXTENSION*)pDeviceObject->DeviceExtension;

    pStack = IoGetCurrentIrpStackLocation(pIrp);
    Length = pStack->Parameters.Read.Length;
    pBuffer = pIrp->AssociatedIrp.SystemBuffer;

    /////////////////////////////////////////
    // 가상하드웨어 Rx 작업

    // polling
    do
    {
        dwData = READ_REGISTER_ULONG((ULONG*)pDeviceExtension->pRegStatus);
        pRegStatusData = (REG_STATUS*)&dwData;
    } while (pRegStatusData->ReadTriggered == 0);

    dwData = READ_REGISTER_ULONG((ULONG*)pDeviceExtension->pRegStatus);
    pRegStatusData = (REG_STATUS*)&dwData;

    if (Length < pRegStatusData->ReadSize)
    {
        ntStatus = STATUS_BUFFER_OVERFLOW;
        goto exit;
    }
    Length = pRegStatusData->ReadSize;

    READ_REGISTER_BUFFER_UCHAR(
        pDeviceExtension->pRxData,
        pBuffer,
        Length
    );

    dwData = READ_REGISTER_ULONG((ULONG*)pDeviceExtension->pRegControl);
    pRegControlData = (REG_CONTROL*)&dwData;
    pRegControlData->DeviceClr = 1;
    WRITE_REGISTER_ULONG((ULONG*)pDeviceExtension->pRegControl, dwData);

    ntStatus = STATUS_SUCCESS;
    Information = Length; 

exit:
    pIrp->IoStatus.Status = ntStatus;
    pIrp->IoStatus.Information = Information;
    IoCompleteRequest(pIrp, 0); // 0 -> IO_NO_INCREMENT
    return ntStatus;
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

	// DeviceObject 삭제
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

    // Start virtual hardware operation
    dwData = READ_REGISTER_ULONG((ULONG*)pDeviceExtension->pRegControl);
    pRegControlData = (REG_CONTROL*)&dwData;
    pRegControlData->DeviceEnable = 1;
    WRITE_REGISTER_ULONG((ULONG*)pDeviceExtension->pRegControl, dwData);

exit:
	return ntStatus;
}
