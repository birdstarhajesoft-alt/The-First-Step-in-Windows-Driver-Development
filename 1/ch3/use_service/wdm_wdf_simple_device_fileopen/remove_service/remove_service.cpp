#include <windows.h>
#include <stdio.h>
#include <conio.h>

#include "..\common\service_common.h"

int main()
{
    BOOLEAN bRet = FALSE;

    bRet = UninstallService(
        (PWSTR)MYDRIVER_SERVICENAME
    );

    if (bRet == TRUE)
        wprintf(L"Successfully removed!\n");
    else
        wprintf(L"Removing Failed!\n");

    wprintf(L"Press any key\n");
    _getch();

    return 0;
}

