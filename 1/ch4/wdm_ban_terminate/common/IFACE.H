#ifndef _IFACE_H_
#define _IFACE_H_

#define BASE_CTLCODE    0x800

#define IOCTL_SIMPLE_SET_PROCESSID		\
	CTL_CODE(							\
		FILE_DEVICE_UNKNOWN,			\
		BASE_CTLCODE + 0,				\
		METHOD_BUFFERED,				\
		FILE_ANY_ACCESS)

#endif // _IFACE_H_
