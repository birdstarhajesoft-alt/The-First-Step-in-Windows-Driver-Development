#pragma comment(lib, "Advapi32.lib")

#ifndef _SERVICE_COMMON_H
#define _SERVICE_COMMON_H

#define MYDRIVER_SERVICENAME            L"WDM_REG_MONITOR"
#define MYDRIVER_SERVICEDISPLAYNAME     L"WDM_REG_MONITOR Driver"
#define MYDRIVER_FILENAME               L"\\WDM_REG_MONITOR.SYS"

BOOLEAN InstallService(
    _In_ PWSTR pszServiceName,
    _In_ PWSTR pszDisplayName,
    _In_ PWSTR szPath,
    _In_ DWORD dwStartType
);

BOOLEAN StartService(
    _In_ PWSTR pszServiceName
);

BOOLEAN StopService(
    _In_ PWSTR pszServiceName
);

BOOLEAN UninstallService(
    _In_ PWSTR pszServiceName
);

#endif //_SERVICE_COMMON_H