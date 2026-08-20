#pragma once

#define		HW_TX_BUFFER_SIZE	(0x00000400)
#define		HW_RX_BUFFER_SIZE	(0x00000400)

#define		HW_REG_CONTROL	(0x00000000)
#pragma pack(1)
typedef struct _REG_CONTROL
{
	ULONG	DeviceEnable		: 1;
	ULONG	DeviceReqIRQ		: 1;
	ULONG	DeviceClr			: 1;
	ULONG	WriteTrigger		: 1;
	ULONG	Reserved1			: 4;
	ULONG	WriteSize			: 16;
	ULONG	Reserved2			: 8;
}REG_CONTROL;
#pragma pack()

#define		HW_REG_STATUS	(0x00000004)
#pragma pack(1)
typedef struct _REG_STATUS
{
	ULONG	DeviceEnabled	: 1;
	ULONG	DeviceAckIRQ	: 1;
	ULONG	ReadTriggered	: 1;
	ULONG	Reserved1		: 5;
	ULONG	ReadSize		: 16;
	ULONG	Reserved2		: 8;
}REG_STATUS;
#pragma pack()

#define		HW_REG_BUFFER	(0x00000008)
#pragma pack(1)
typedef struct _REG_BUFFER
{
	unsigned char TxData[HW_TX_BUFFER_SIZE];
	unsigned char RxData[HW_RX_BUFFER_SIZE];
}REG_BUFFER;
#pragma pack()

typedef struct _CONTROLLER
{
	ULONG_PTR	ullPhysical_ControllerRegisterBase;
	ULONG		ulPhysical_ControllerRegisterSize;
	ULONG		ulInterruptVector;
	KIRQL		Irql;
	void*		pReserved;
}CONTROLLER;

BOOLEAN hardware_Initialize(CONTROLLER* pController);
void hardware_Termination(CONTROLLER* pController);