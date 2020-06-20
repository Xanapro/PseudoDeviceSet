#pragma once

#if !defined(MAKE_BYTE) && !defined(MAKE_DWORD)
#define MAKE_BYTE(b) ((b) & 0xFF)
#define MAKE_DWORD(a,b,c,d) ((MAKE_BYTE(a) << 24) | (MAKE_BYTE(b) << 16) | (MAKE_BYTE(c) << 8) | MAKE_BYTE(d))
#endif//~!defined(MAKE_BYTE) && !defined(MAKE_DWORD)

// /opt/Windows-driver-samples/network/wlan/WDI/PLATFORM/NDIS6\SDIO/N6Sdio_typedef.h

#define BUS_VERSION MAKE_DWORD(1, 29, 70, 0)

// {AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA}
DEFINE_GUID(GUID_DEVINTERFACE_PSEUDOBUS, 0xAAAAAAAA, 0xAAAA, 0xAAAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA);

typedef struct _t_XinputGamePadClone	// sa: XInput.h, https://docs.microsoft.com/en-us/windows/win32/api/xinput/ns-xinput-_xinput_gamepad
{
/////
	WORD	cbSize;
	//
    WORD	wButtons;
    BYTE	bLeftTrigger;
    BYTE	bRightTrigger;
    SHORT	sThumbLX;
    SHORT	sThumbLY;
    SHORT	sThumbRX;
    SHORT	sThumbRY;
} t_XinputGamePadClone;// XINPUT_GAMEPAD, *PXINPUT_GAMEPAD;
// 12 byte


typedef struct _t_XIFeedback	// sa: XINPUT_VIBRATION
{
	WORD	wLeftMotorSpeed;
	WORD	wRightMotorSpeed;
	WORD	pad0[2];
	// 8

	WORD	wRingLight;
	// 10

	BYTE pad1[6];
} t_XIFeedback;



typedef struct _t_XIPadData {
	t_XIFeedback feedback;
	// 10
	t_XinputGamePadClone	xigpc;
} t_XIPadData;
// 22 byte


#if defined(_X86_)
#	define aligned__	(4)
#else// | defined(_X86_)
#	define aligned__	(8)
#endif//~defined(_X86_)

#if !defined(ALIGN)
#define ALIGN(size__) (size__?(((size__+(aligned__-1))/aligned__)*aligned__):aligned__)
#endif//!defined(ALIGN)
//#undef aligned__

//
//  Data structures used in User IoCtls
//
typedef struct _t_BusEnumVersionRes {
	__out DWORD	dwVersion;
	DWORD		pad0;
} t_BusEnumVersionRes;
typedef struct _t_BusEnumFindEmptySlotRes {
	__out BYTE	byEmptySlot;
	__out_bcount(7) BYTE	pad[7];
} t_BusEnumFindEmptySlotRes;

typedef struct _t_BusEnumProcIDReq {
	__in BYTE	byIdx;
	__in_bcount(7) BYTE		pad[7];
} t_BusEnumProcIDReq;
typedef struct _t_BusEnumProcIDRep {
	__out DWORD dwPID;
	__out DWORD	pad0;
} t_BusEnumProcIDRep;

typedef struct _t_BusEnumIsPlugedReq {
	__in BYTE	byIdx;
	__in_bcount(7) BYTE		pad[7];
} t_BusEnumIsPlugedReq;
typedef struct _t_BusEnumIsPlugedRep {
	__out BYTE	byState;
	__out_bcount(7) BYTE		pad[7];
} t_BusEnumIsPlugedRep;


typedef struct _t_BusEnumPluginReq {
	__in BYTE	byIdx;
	__in_bcount(2) BYTE		pad[2];
	__in_bcount(5) BYTE		byIndividualCode[5];		//
	__in_bcount(128) BYTE	byaKey[128];
} t_BusEnumPluginReq;
/*
typedef struct _t_BusEnumPluginRep {
	__in BYTE	byState;
	BYTE		pad[7];
} t_BusEnumPluginRep;
*/

typedef struct _t_BusEnumUnplugReq {
	__in BYTE	byIdx;
	__in BYTE	byAll;
	__in BYTE	byForce;
	__in_bcount(5) BYTE		byIndividualCode[5];
} t_BusEnumUnplugReq;
/*
typedef struct _t_BusEnumUnplugRep {
	__in BYTE	byState;
	BYTE		pad[7];
} t_BusEnumUnplugRep;
*/


typedef struct _t_BusEnumReportReq {
	__in BYTE m_bySize;
	__in BYTE m_byIdx;
	// 2
	__in_bcount(sizeof (t_XinputGamePadClone)) BYTE m_byData[sizeof (t_XinputGamePadClone)];
	// 12
	
///	__in_bcount(7) BYTE		pad[2];
} t_BusEnumReportReq;

typedef struct _t_BusEnumReportRep {
	__out t_XIFeedback	m_feedback;
} t_BusEnumReportRep;




////////////////////////////////////////////////////
enum e_ProtocolID {
	E_PROTO_NONE	= 0x70,

	E_PROTO_PLUGIN,
	E_PROTO_UNPLUG,
	E_PROTO_REPORT,
	E_PROTO_IS_PLUGED,
	E_PROTO_PROC_ID,
	E_PROTO_FIND_EMPTY_SLOT,
	E_PROTO_VER,

	E_PROTO_MAX		= 0xFF,
};
inline char* protoid2human(/*e_ProtocolID*/int id)
{
	switch (id) {
	case E_PROTO_NONE:				return (char*)("E_PROTO_NONE");
	case E_PROTO_PLUGIN:			return (char*)("E_PROTO_PLUGIN");
	case E_PROTO_UNPLUG:			return (char*)("E_PROTO_UNPLUG");
	case E_PROTO_REPORT:			return (char*)("E_PROTO_REPORT");
	case E_PROTO_IS_PLUGED:			return (char*)("E_PROTO_IS_PLUGED");
	case E_PROTO_PROC_ID:			return (char*)("E_PROTO_PROC_ID");
	case E_PROTO_FIND_EMPTY_SLOT:	return (char*)("E_PROTO_FIND_EMPTY_SLOT");
	case E_PROTO_VER:				return (char*)("E_PROTO_VER");
	case E_PROTO_MAX:				return (char*)("E_PROTO_MAX");
	}//~hctiws
	return ((char*)"UNDEFINED PROTOCOL ID");
}


typedef struct _t_ProtocolHdr {
	__in BYTE	byProtocolIdx;		// REFER: e_ProtocolID
	__in BYTE	byChunkSize;
	__in_bcount(6) BYTE	pad[6];
} t_ProtocolHdr;
//////////////
// t_ProtocoHdr	: 1byte
//		t_ProtocolHDr.byChunkSize .....
//



