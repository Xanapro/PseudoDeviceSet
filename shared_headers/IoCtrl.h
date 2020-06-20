#pragma once

/// see also: Windows-driver-samples/general/toastDrv/inc/pubic.h

#define BUS_ENUM_W_IOCTL(v)			CTL_CODE(FILE_DEVICE_BUS_EXTENDER, v, METHOD_BUFFERED, FILE_WRITE_DATA)
#define BUS_ENUM_R_IOCTL(v)			CTL_CODE(FILE_DEVICE_BUS_EXTENDER, v, METHOD_BUFFERED, FILE_READ_DATA)
#define BUS_ENUM_RW_IOCTL(v)		CTL_CODE(FILE_DEVICE_BUS_EXTENDER, v, METHOD_BUFFERED, FILE_WRITE_DATA | FILE_READ_DATA)

//#define CTL_CODE( DeviceType, Function, Method, Access ) (                 \
//    ((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method) \
//)
#define IOCTL_BUS_PROTOCOL				BUS_ENUM_RW_IOCTL(0xFF00)
