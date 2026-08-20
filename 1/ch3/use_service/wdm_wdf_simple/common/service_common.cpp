#include <windows.h>
#include <stdio.h>
#include <conio.h>

#include "..\common\service_common.h"

BOOLEAN InstallService(
    _In_ PWSTR pszServiceName,
    _In_ PWSTR pszDisplayName,
    _In_ PWSTR szPath,
    _In_ DWORD dwStartType)
{
    BOOLEAN bRet = FALSE;
    SC_HANDLE schSCManager = nullptr;
    SC_HANDLE schService = nullptr;

    // Open the local default service control manager database
    schSCManager = OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT |
        SC_MANAGER_CREATE_SERVICE);
    if (schSCManager == nullptr)
    {
        wprintf(L"OpenSCManager failed w/err 0x%08lx\n", GetLastError());
        goto Cleanup;
    }

    // Install the service into SCM by calling CreateService
    schService = CreateService(
        schSCManager,                   // SCManager database
        pszServiceName,                 // Name of service
        pszDisplayName,                 // Name to display
        SERVICE_QUERY_STATUS | SERVICE_START,  // Desired access
        SERVICE_KERNEL_DRIVER,      // Service type
        dwStartType,                    // Service start type
        SERVICE_ERROR_NORMAL,           // Error control type
        szPath,                         // Service's binary
        nullptr,                           // No load ordering group
        nullptr,                           // No tag identifier
        NULL,                           // Dependencies
        NULL,                     // Service running account
        NULL                     // Password of the account
    );

    if (schService == nullptr)
    {
        wprintf(L"CreateService failed w/err 0x%08lx\n", GetLastError());
        goto Cleanup;
    }

    bRet = TRUE;

Cleanup:
    // Centralized cleanup for all allocated resources.
    if (schSCManager)
    {
        CloseServiceHandle(schSCManager);
        schSCManager = nullptr;
    }
    if (schService)
    {
        CloseServiceHandle(schService);
        schService = nullptr;
    }
    return bRet;
}

BOOLEAN StartService(
    _In_ PWSTR pszServiceName)
{
    BOOLEAN bRet = FALSE;
    SC_HANDLE schSCManager = nullptr;
    SC_HANDLE schService = nullptr;

    // Open the local default service control manager database
    schSCManager = OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT |
        SC_MANAGER_CREATE_SERVICE);
    if (schSCManager == nullptr)
    {
        wprintf(L"OpenSCManager failed w/err 0x%08lx\n", GetLastError());
        goto Cleanup;
    }

    schService = OpenService(schSCManager, pszServiceName, SERVICE_START |
        SERVICE_QUERY_STATUS | DELETE);

    if (schService == nullptr)
    {
        wprintf(L"OpenService failed w/err 0x%08lx\n", GetLastError());
        goto Cleanup;
    }

    if (StartService(schService, 0, nullptr) == 0)
    {
        wprintf(L"StartService failed w/err 0x%08lx\n", GetLastError());
        goto Cleanup;
    }

    bRet = TRUE;

Cleanup:
    // Centralized cleanup for all allocated resources.
    if (schSCManager)
    {
        CloseServiceHandle(schSCManager);
        schSCManager = nullptr;
    }
    if (schService)
    {
        CloseServiceHandle(schService);
        schService = nullptr;
    }
    return bRet;
}

BOOLEAN StopService(_In_ PWSTR pszServiceName)
{
    BOOLEAN bRet = FALSE;
    SC_HANDLE schSCManager = nullptr;
    SC_HANDLE schService = nullptr;
    SERVICE_STATUS ssSvcStatus = {};

    // Open the local default service control manager database
    schSCManager = OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (schSCManager == nullptr)
    {
        wprintf(L"OpenSCManager failed w/err 0x%08lx\n", GetLastError());
        goto Cleanup;
    }

    // Open the service with delete, stop, and query status permissions
    schService = OpenService(schSCManager, pszServiceName, SERVICE_STOP |
        SERVICE_QUERY_STATUS | DELETE);
    if (schService == nullptr)
    {
        wprintf(L"OpenService failed w/err 0x%08lx\n", GetLastError());
        goto Cleanup;
    }

    // Try to stop the service
    if (ControlService(schService, SERVICE_CONTROL_STOP, &ssSvcStatus))
    {
        wprintf(L"Stopping %s.", pszServiceName);
        Sleep(1000);

        while (QueryServiceStatus(schService, &ssSvcStatus))
        {
            if (ssSvcStatus.dwCurrentState == SERVICE_STOP_PENDING)
            {
                wprintf(L".");
                Sleep(1000);
            }
            else break;
        }
    }

    bRet = TRUE;

Cleanup:
    // Centralized cleanup for all allocated resources.
    if (schSCManager)
    {
        CloseServiceHandle(schSCManager);
        schSCManager = nullptr;
    }
    if (schService)
    {
        CloseServiceHandle(schService);
        schService = nullptr;
    }
    return bRet;
}

BOOLEAN UninstallService(_In_ PWSTR pszServiceName)
{
    BOOLEAN bRet = FALSE;
    SC_HANDLE schSCManager = nullptr;
    SC_HANDLE schService = nullptr;
    SERVICE_STATUS ssSvcStatus = {};

    // Open the local default service control manager database
    schSCManager = OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (schSCManager == nullptr)
    {
        wprintf(L"OpenSCManager failed w/err 0x%08lx\n", GetLastError());
        goto Cleanup;
    }

    // Open the service with delete, stop, and query status permissions
    schService = OpenService(schSCManager, pszServiceName, SERVICE_STOP |
        SERVICE_QUERY_STATUS | DELETE);
    if (schService == nullptr)
    {
        wprintf(L"OpenService failed w/err 0x%08lx\n", GetLastError());
        goto Cleanup;
    }

    if (!DeleteService(schService))
    {
        wprintf(L"DeleteService failed w/err 0x%08lx\n", GetLastError());
        goto Cleanup;
    }

    bRet = TRUE;

Cleanup:
    // Centralized cleanup for all allocated resources.
    if (schSCManager)
    {
        CloseServiceHandle(schSCManager);
        schSCManager = nullptr;
    }
    if (schService)
    {
        CloseServiceHandle(schService);
        schService = nullptr;
    }
    return bRet;
}
