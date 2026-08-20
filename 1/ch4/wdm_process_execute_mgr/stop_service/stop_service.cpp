#include <windows.h>
#include <stdio.h>
#include <conio.h>

#include "..\common\service_common.h"

int main()
{
    BOOLEAN bRet = FALSE;

    bRet = StopService(
        (PWSTR)MYDRIVER_SERVICENAME
    );

    wprintf(L"\n");

    if (bRet == TRUE)
        wprintf(L"Successfully stopped!\n");
    else
        wprintf(L"Stopping Failed!\n");

    wprintf(L"Press any key\n");
    _getch();

    return 0;
}

