#include <ntddk.h>
#include <wdf.h>
#include <wdmsec.h> // for SDDLs
#include "..\common\namespace.h"

#define MAX_DEVICE_COUNT	(1)

// Arbitrary structure defined by the developer
typedef struct _MY_DEVICE_EXTENSION
{
    int times; // example
}MY_DEVICE_EXTENSION, * PMY_DEVICE_EXTENSION;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(MY_DEVICE_EXTENSION, DeviceGetExtension)

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
    int times = 0;
    WDF_DRIVER_CONFIG config;
    NTSTATUS status;
    PWDFDEVICE_INIT  deviceInit = NULL;
    WDFDRIVER WdfDriver;
    WDF_OBJECT_ATTRIBUTES deviceAttributes;
    PMY_DEVICE_EXTENSION pMyDeviceExtension = NULL;
    WDFDEVICE WdfDevice;
    UNICODE_STRING UniDeviceName;
    UNICODE_STRING GlobalDeviceName;

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, MY_DEVICE_EXTENSION);

    WDF_DRIVER_CONFIG_INIT(&config,
        WDF_NO_EVENT_CALLBACK
    );

    config.DriverInitFlags |= WdfDriverInitNonPnpDriver;
    config.EvtDriverUnload = MyDriverUnload;

    status = WdfDriverCreate(DriverObject,
                             RegistryPath,
                             WDF_NO_OBJECT_ATTRIBUTES,
                             &config,
                             &WdfDriver
                             );

    if (!NT_SUCCESS(status)) {
        goto exit;
    }

    RtlInitUnicodeString(&UniDeviceName, NT_NAME_DEVICE);
    RtlInitUnicodeString(&GlobalDeviceName, NT_NAME_GLOBAL);

    for (times = 0; times < MAX_DEVICE_COUNT; times++)
    {
        deviceInit = WdfControlDeviceInitAllocate(
            WdfDriver,
            &SDDL_DEVOBJ_SYS_ALL_ADM_RWX_WORLD_RWX_RES_RWX    // All clients can use this device
        );
        if (deviceInit == NULL) {
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto exit;
        }

        // WDFDEVICE's NT Namespace
        status = WdfDeviceInitAssignName(deviceInit, &UniDeviceName);
        if (!NT_SUCCESS(status)) {
            WdfDeviceInitFree(deviceInit);
            goto exit;
        }

        status = WdfDeviceCreate(&deviceInit, &deviceAttributes, &WdfDevice);
        if (!NT_SUCCESS(status)) {
            WdfDeviceInitFree(deviceInit);
            goto exit;
        }

        status = WdfDeviceCreateSymbolicLink(WdfDevice, &GlobalDeviceName);
        if (!NT_SUCCESS(status))
        {
            goto exit;
        }

        pMyDeviceExtension = DeviceGetExtension(WdfDevice);
        pMyDeviceExtension->times = times;
        WdfControlFinishInitializing(WdfDevice);
    }

exit:
    return status;
}

