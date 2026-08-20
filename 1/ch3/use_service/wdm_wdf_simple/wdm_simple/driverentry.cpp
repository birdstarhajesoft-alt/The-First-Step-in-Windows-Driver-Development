#include <ntddk.h>

VOID
MyDriverUnload(
	PDRIVER_OBJECT pDriverObject
)
{
	UNREFERENCED_PARAMETER(pDriverObject);
}

extern "C" NTSTATUS DriverEntry(
	PDRIVER_OBJECT pDriverObject,
	PUNICODE_STRING pRegPath
)
{
	UNREFERENCED_PARAMETER(pRegPath);
	pDriverObject->DriverUnload = MyDriverUnload;

	return STATUS_SUCCESS;
}