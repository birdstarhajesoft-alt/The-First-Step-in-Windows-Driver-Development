#include <windows.h>
#include <stdio.h>
#include <conio.h>

#include "..\common\service_common.h"

int main()
{
    WCHAR DriverFilePath[MAX_PATH];
    BOOLEAN bRet = FALSE;

    GetCurrentDirectory(MAX_PATH, DriverFilePath);
    wcscat_s(DriverFilePath, MYDRIVER_FILENAME);

    bRet = InstallService(
        (PWSTR)MYDRIVER_SERVICENAME,
        (PWSTR)MYDRIVER_SERVICEDISPLAYNAME,
        DriverFilePath,
        SERVICE_DEMAND_START
    );

    if (bRet == TRUE)
        wprintf(L"Successfully installed!\n");
    else
        wprintf(L"Installing Failed!\n");

    wprintf(L"Press any key\n");
    _getch();

    return 0;
}

