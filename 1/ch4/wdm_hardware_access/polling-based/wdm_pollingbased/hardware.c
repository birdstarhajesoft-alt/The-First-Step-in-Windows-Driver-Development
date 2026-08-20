#include <ntddk.h>
#include "hardware.h"

#define REGISTER_SIZE		(0x1000)

KDPC g_Dpc;
KTIMER g_Timer;
BOOLEAN g_bReqTerminate = FALSE;
BOOLEAN g_bAckTerminated = FALSE;

REG_CONTROL* g_pRegControl = NULL;
REG_STATUS* g_pRegStatus = NULL;
unsigned char* g_pTxData = NULL;
unsigned char* g_pRxData = NULL;

void gen_int(ULONG Vector);

VOID
DpcRoutine(
	struct _KDPC* Dpc,
	PVOID  DeferredContext,
	PVOID  SystemArgument1,
	PVOID  SystemArgument2
)
{
	LARGE_INTEGER DueTime;
	ULONG dwSize = 0;
	KIRQL OldIrql;
	ULONG count;
	CONTROLLER* pController = NULL;

	UNREFERENCED_PARAMETER(SystemArgument1);
	UNREFERENCED_PARAMETER(SystemArgument2);

	pController = (CONTROLLER*)DeferredContext;

	if (g_bReqTerminate == TRUE)
	{
		g_bAckTerminated = TRUE;
		goto exit;
	}

	// action..
	if (g_pRegControl->DeviceEnable)
		g_pRegStatus->DeviceEnabled = 1;
	else
	{
		g_pRegControl->WriteSize = 0;
		g_pRegStatus->ReadSize = 0;
		g_pRegStatus->ReadTriggered = 0;
		g_pRegControl->WriteTrigger = 0;
		g_pRegControl->DeviceClr = 0;
		g_pRegStatus->DeviceAckIRQ = 0;

		DueTime.QuadPart = -1 * 1000000;
		KeSetTimer(&g_Timer, DueTime, Dpc);
		goto exit;
	}

	if (g_pRegControl->DeviceClr)
	{
		g_pRegControl->WriteSize = 0;
		g_pRegStatus->ReadSize = 0;
		g_pRegStatus->ReadTriggered = 0;
		g_pRegControl->WriteTrigger = 0;
		g_pRegControl->DeviceClr = 0;
		g_pRegStatus->DeviceAckIRQ = 0;
	}

	if (g_pRegControl->WriteTrigger)
	{
		if (g_pRegControl->WriteSize > HW_TX_BUFFER_SIZE)
			dwSize = HW_TX_BUFFER_SIZE;
		else
			dwSize = g_pRegControl->WriteSize;

		if (dwSize)
		{
			for (count = 0; count < dwSize; count++)
			{
				if (g_pTxData[count] < 'A')
					g_pRxData[count] = g_pTxData[count];
				else if (g_pTxData[count] <= 'Z')
					g_pRxData[count] = g_pTxData[count] + 0x20;
				else if (g_pTxData[count] < 'a')
					g_pRxData[count] = g_pTxData[count];
				else if (g_pTxData[count] <= 'z')
					g_pRxData[count] = g_pTxData[count] - 0x20;
			}
		}
		g_pRegControl->WriteTrigger = 0;
		g_pRegControl->WriteSize = dwSize;
		g_pRegStatus->ReadSize = dwSize;
		g_pRegStatus->ReadTriggered = 1;

		if ((g_pRegStatus->DeviceAckIRQ == 0) && (g_pRegControl->DeviceReqIRQ))
		{
			g_pRegStatus->DeviceAckIRQ = 1;
			if (pController->ulInterruptVector)
			{
				KeRaiseIrql(pController->Irql, &OldIrql);
				gen_int(pController->ulInterruptVector);
				KeLowerIrql(OldIrql);
			}
		}
	}

	DueTime.QuadPart = -1 * 1000000;
	KeSetTimer(&g_Timer, DueTime, Dpc);

exit:
	return;
}

BOOLEAN
MyDummyISR(
	__in struct _KINTERRUPT* Interrupt,
	__in PVOID  ServiceContext
)
{
	UNREFERENCED_PARAMETER(Interrupt);
	UNREFERENCED_PARAMETER(ServiceContext);
	return FALSE;
}

BOOLEAN hardware_Initialize(CONTROLLER * pController)
{
	NTSTATUS ntStatus;
	BOOLEAN bRet = FALSE;
	SIZE_T  NumberOfBytes = REGISTER_SIZE;
	PHYSICAL_ADDRESS LowestAcceptableAddress;
	PHYSICAL_ADDRESS HighestAcceptableAddress;
	PHYSICAL_ADDRESS BoundaryAddressMultiple;
	PHYSICAL_ADDRESS ReturnAddress;
	LARGE_INTEGER DueTime;
	PKINTERRUPT pKInterrupt = NULL;
	ULONG Vector;
	KIRQL Irql;

	if (pController == NULL)
		goto exit;

	LowestAcceptableAddress.QuadPart = 0;
	HighestAcceptableAddress.QuadPart = -1;
	BoundaryAddressMultiple.QuadPart = REGISTER_SIZE;

	pController->pReserved = MmAllocateContiguousMemorySpecifyCache(
			NumberOfBytes,
			LowestAcceptableAddress,
			HighestAcceptableAddress,
			BoundaryAddressMultiple,
			MmNonCached
		);

	if (pController->pReserved == NULL)
		goto exit;

	memset(pController->pReserved, 0, NumberOfBytes);

	pController->ulPhysical_ControllerRegisterSize = REGISTER_SIZE;
	ReturnAddress = MmGetPhysicalAddress(pController->pReserved);
	pController->ullPhysical_ControllerRegisterBase = ReturnAddress.QuadPart;

	KeInitializeDpc(&g_Dpc, DpcRoutine, pController);
	KeInitializeTimer(&g_Timer);

	g_pRegControl = (REG_CONTROL * )((unsigned char *)(pController->pReserved) + HW_REG_CONTROL);
	g_pRegStatus = (REG_STATUS*)((unsigned char*)(pController->pReserved) + HW_REG_STATUS);
	g_pTxData = (unsigned char*)(pController->pReserved) + HW_REG_BUFFER;
	g_pRxData = g_pTxData + HW_TX_BUFFER_SIZE;

	// 가상하드웨어 ISR 설치
	pController->ulInterruptVector = 0;
	pController->Irql = 0;

	for (Irql = 5; Irql <= 8; Irql++)
	{
		Vector = Irql * 0x10;
		ntStatus =
			IoConnectInterrupt(
				&pKInterrupt,
				MyDummyISR,
				NULL,
				NULL,
				Vector,
				Irql,
				Irql,
				LevelSensitive,
				TRUE,
				1,
				FALSE
			);
		if (NT_SUCCESS(ntStatus))
		{
			IoDisconnectInterrupt(pKInterrupt);
			pController->ulInterruptVector = Vector;
			pController->Irql = Irql;
			break;
		}
	}

	DueTime.QuadPart = -1 * 1000000;
	KeSetTimer(&g_Timer, DueTime, &g_Dpc);

	bRet = TRUE;

exit:
	return bRet;
}

void hardware_Termination(CONTROLLER* pController)
{
	LARGE_INTEGER  Interval;

	Interval.QuadPart = -1 * 2000000;

	g_pRegControl->WriteSize = 0;
	g_pRegStatus->ReadSize = 0;
	g_pRegStatus->ReadTriggered = 0;
	g_pRegControl->WriteTrigger = 0;
	g_pRegControl->DeviceClr = 0;
	g_pRegStatus->DeviceAckIRQ = 0;
	g_pRegControl->DeviceReqIRQ = 0;

	while (1)
	{
		g_bReqTerminate = TRUE;
		KeDelayExecutionThread(KernelMode, FALSE, &Interval);
		if (g_bAckTerminated == TRUE)
			break;
	}

	if (pController == NULL)
		goto exit;

	if(pController->pReserved)
		MmFreeContiguousMemory(pController->pReserved);

exit:
	return;
}