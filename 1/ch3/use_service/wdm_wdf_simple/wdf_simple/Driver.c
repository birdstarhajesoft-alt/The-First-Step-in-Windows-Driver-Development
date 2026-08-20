#include <ntddk.h>
#include <wdf.h>

VOID
MyDriverUnload(
    WDFDRIVER Driver
)
{
    UNREFERENCED_PARAMETER(Driver);
}

NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT  DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    )
{
    WDF_DRIVER_CONFIG config;
    NTSTATUS status;

    WDF_DRIVER_CONFIG_INIT(&config,
        WDF_NO_EVENT_CALLBACK
    );

    config.DriverInitFlags |= WdfDriverInitNonPnpDriver;
    config.EvtDriverUnload = MyDriverUnload;

    status = WdfDriverCreate(DriverObject,
                             RegistryPath,
                             WDF_NO_OBJECT_ATTRIBUTES,
                             &config,
                             WDF_NO_HANDLE
                             );

    if (!NT_SUCCESS(status)) {
        return status;
    }

    return status;
}

