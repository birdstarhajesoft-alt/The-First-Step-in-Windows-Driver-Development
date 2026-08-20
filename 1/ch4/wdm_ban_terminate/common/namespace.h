#ifndef _NAMESPACE_H_

#define MYDEVICE_NAME	L"BAN_TERMINATE_PROCESS"

////////////////////////////////////////////
/* Use for Application*/
//

// Win32 Namespace Name Defines
#define WIN32_NAME	L"\\\\.\\" MYDEVICE_NAME

//
////////////////////////////////////////////

////////////////////////////////////////////
/* Use for Driver*/
//

// NT Namespace For Global??(Win32 Symbolic)
#define NT_NAME_GLOBAL	L"\\DosDevices\\" MYDEVICE_NAME

// NT Namespace For Device
#define NT_NAME_DEVICE	L"\\Device\\" MYDEVICE_NAME

//
////////////////////////////////////////////

#endif //_NAMESPACE_H_