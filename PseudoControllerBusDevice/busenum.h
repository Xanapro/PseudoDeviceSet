#pragma once


#include <ntddk.h>
#include <wdm.h>

#include <wmilib.h>
#include <initguid.h>
#include <ntintsafe.h>
#include <wdmsec.h>
#include <wdmguid.h>
#include <usbdrivr.h>

#include "./PseudoControllerBusDevice.h"

#define NTSTRSAFE_LIB
#include <ntstrsafe.h>
#include <dontuse.h>

#include "./ntmiscdbg.h"



////////////////////////
// IoCreateDeviceSecure()
// {BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB}
DEFINE_GUID(GUID_DEVCLASS_PSEUDO_BUS_DEVICE, 0xBBBBBBBB, 0xBBBB, 0xBBBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB);



#define K_BUSENUM_POOL_TAG		(ULONG)'AAAA'

// XInput1_4.dll

// HKEY_USERS/{GUID}/System/CurrentControlSet/Control/MediaProperties/PrivateProperties/DirectInput/
// sa: C:\opt\Windows-driver-samples\general\toaster\toastDrv\inc\public.h
// sa: /opt/Windows-driver-samples/general/cancel/sys/cancel.c
// HKLM/SYSTEM/DriverDatabase/DeviceIds/USB/MS_COMP_USB_10, 20
#define K_DEVICE_HARDWARE_ID	L"Root\\PseudoControllerBusDevice\0"

// devman: Configuration ID
#define K_BUSENUM_COMPATIBLE_IDS	L"USB\\MS_COMP_XUSB10\0"	// xusb22.inf:USB\MS_COMP_XUSB10,CC_Install 0x028E
///#define K_BUSENUM_COMPATIBLE_IDS	L"USB\\MS_COMP_XUSB20\0"	// xusb22.inf:USB\MS_COMP_XUSB20,CC_Install 0x27DC
///#define K_BUSENUM_COMPATIBLE_IDS	L"USB\\VID_045E&PID_028E\0"	// xusb22.inf:USB\VID_045E&PID_028E,CC_Install


///#define K_USB_VENDOR_ID	(0x16C0)	// sa: https://github.com/obdev/v-usb/blob/master/usbdrv/USB-IDs-for-free.txt
#define K_USB_VENDOR_ID	(0xCCCC)	// XANAPRO USB\VID_327C&PID_028E&IG_00\2&b46c1b0&0&00

#define K_PRODUCT    L"Puseudo Controller"
#define K_VENDORNAME L"YourVendor "
#define K_MODEL      L"Pseudo Bus : #"


#define K_INDIVIDUAL_HASH		("00000000000000000000")

// C:\opt\Windows-driver-samples\tools\dv\samples\DV-FailDriver-WDM
#define DRIVER_TAG 'NOMT'

// sa: /opt/Windows-driver-samples/tools/dv/samples/DV-FailDriver-WDM/defect_toastmon.c
#define INITIALIZE_PNP_STATE(_Data_)    \
        (_Data_)->m_eDevicePnPState =  NotStarted;\
        (_Data_)->m_ePreviousPnPState = NotStarted;

#define SET_NEW_PNP_STATE(_Data_, _state_) \
        (_Data_)->m_ePreviousPnPState =  (_Data_)->m_eDevicePnPState;\
        (_Data_)->m_eDevicePnPState = (_state_);

#define RESTORE_PREVIOUS_PNP_STATE(_Data_)   \
        (_Data_)->m_eDevicePnPState =   (_Data_)->m_ePreviousPnPState;\


#if defined(_X86_)
#	define CONFIGURATION_SIZE	0x00E4
#else// | defined(_X86_)
#	define CONFIGURATION_SIZE	0x0130
#endif//~defined(_X86_)


inline NTSTATUS irpcopy(IRP* pIRP, void* pSrc, size_t szSrcLen, size_t szOutputLen)
{
	// RPC_NT_NULL_REF_POINTER
	if (szSrcLen < szOutputLen) {
		TEXEC(1, fstr("___ srcLen: 0x%08X, dstLen: 0x%08X\n", szSrcLen, szOutputLen));
		return (STATUS_INVALID_USER_BUFFER);
	}
	memcpy(pIRP->AssociatedIrp.SystemBuffer, pSrc, szSrcLen);
	pIRP->IoStatus.Information = szSrcLen;
	return (STATUS_SUCCESS);
}


#define K_QUADRANT_LIGHT_LEN	(3)

// see also: C:\opt\Windows-driver-samples\tools\dv\samples\DV-FailDriver-WDM\defect_toastmon.h
typedef enum _e_DevicePnPState {
	E_DPNP_NOT_STARTED = 0,			// Not started yet
	E_DPNP_STARTED,					// Device has received the START_DEVICE IRP
	E_DPNP_STOP_PENDING,			// Device has received the QUERY_STOP IRP
	E_DPNP_STOPPED,					// Device has received the STOP_DEVICE IRP
	E_DPNP_REMOVE_PENDING,			// Device has received the QUERY_REMOVE IRP
	E_DPNP_SURPRISE_REMOVE_PENDING,	// Device has received the SURPRISE_REMOVE IRP
	E_DPNP_DELETED,					// Device has received the REMOVE_DEVICE IRP
	E_DPNP_UNKNOWN,					// Unknown state
} e_DevicePnPState;

typedef struct _t_Pending_IRP {
	LIST_ENTRY	Link;
	PIRP		Irp;
} t_Pending_IRP;///, *Pt_Pending_IRP;


// REF: /Windows-driver-samples/serial/serenum/serenum.h
typedef struct _t_Cmn_DeviceData
{
	DEVICE_OBJECT*		m_pSelfDeviceObj;	///Self;
	BOOLEAN				m_bIsFDO;
	e_DevicePnPState	m_eDevicePnPState;
	e_DevicePnPState	m_ePreviousPnPState;
	
	SYSTEM_POWER_STATE	m_eSystemPowerState;
	DEVICE_POWER_STATE	m_eDevicePowerState;
} t_Cmn_DeviceData;

typedef struct _t_PDO_DeviceData	// : public t_Cmn_DeviceData
{
#pragma warning(suppress: 4201)
	t_Cmn_DeviceData;
	
	DEVICE_OBJECT*		m_pParentFDO;

	// Text desc
	UNICODE_STRING*		m_puniHardwareID;		//!!! Either in the form of bus\device or *PNPXXXX - meaning root enumerated
	UNICODE_STRING*		m_puniInstanceID;		// Format: bus\device

	ULONG		m_ulPadIdx;
	LIST_ENTRY	Link;
	BOOLEAN		m_bIsPresent;
	BOOLEAN		m_bIsStarted;
	BOOLEAN		m_bIsReportedMissing;
	
	/// ALREADY aligned UCHAR       Reserved[2]; // for 4 byte alignment

	ULONG		m_ulInterfaceRefCount;
	LIST_ENTRY	m_lstentHoldingQueue;
	LIST_ENTRY	m_lstentPendingQueue;
	KSPIN_LOCK	m_lstentPendingQueueLock;
	//@{	// dont replace line
	BYTE		m_byaRumble[sizeof (DWORD64)];	// vib?
	BYTE		m_byRingLightNo;
	BYTE		m_byaReport[sizeof (t_XinputGamePadClone)];
	//@}
	UNICODE_STRING	m_ucInterfaceName;

	///HANDLE m_hCallingPID;
	DWORD		m_dwCallingPID;
	BYTE		m_byIndividualCode[5];
	BYTE		m_byPad[3];

} t_PDO_DeviceData;

typedef struct _t_FDO_DeviceData // : public t_Cmn_DeviceData
{
#pragma warning(suppress: 4201)
	t_Cmn_DeviceData;
	
	DEVICE_OBJECT*	m_pUnderlyingPDO;
	DEVICE_OBJECT*	m_pNextLowerDriver;
	LIST_ENTRY		m_lstentListOfPDOs;
	ULONG			m_ulNumPDOs;
	
	//@{
	FAST_MUTEX		m_Mutex;
	IO_REMOVE_LOCK	m_lockRemove;
	//@}

	ULONG			m_ulOutstandingIO;
	KEVENT			m_evtRemove;
	KEVENT			m_evtStop;
	UNICODE_STRING	m_ucInterfaceName;
} t_FDO_DeviceData;


/////////////////////////////////////////////
//
// Defined in busenum.c
//
DRIVER_INITIALIZE DriverEntry;
DRIVER_UNLOAD Bus_DriverUnload;

__drv_dispatchType(IRP_MJ_CREATE)					DRIVER_DISPATCH Bus_Create;
__drv_dispatchType(IRP_MJ_CLOSE)					DRIVER_DISPATCH Bus_CreateClose;
__drv_dispatchType(IRP_MJ_CLEANUP)					DRIVER_DISPATCH Bus_CleanUp;
__drv_dispatchType(IRP_MJ_DEVICE_CONTROL)			DRIVER_DISPATCH Bus_IoCtl;
__drv_dispatchType(IRP_MJ_INTERNAL_DEVICE_CONTROL)	DRIVER_DISPATCH Bus_Internal_IoCtl;
__drv_dispatchType(IRP_MJ_SYSTEM_CONTROL)			DRIVER_DISPATCH Bus_DispatchSystemControl;

DRIVER_CANCEL CB_CancelIrp;		// CALLBACK

VOID bus_AddRef(__in t_FDO_DeviceData* Data);
VOID bus_ReleaseRef(__in t_FDO_DeviceData* Data);

NTSTATUS protocol_bus_ReportDevice(t_BusEnumReportReq* pReq, t_FDO_DeviceData* fdoData, ULONG ulOutputLen, IRP* pIRP);
NTSTATUS protocol_bus_IsDevicePluggedIn(t_BusEnumIsPlugedReq* pReq, t_FDO_DeviceData* pFDO_DeviceData, ULONG ulOutputLen, IRP* pIRP);
NTSTATUS protocol_bus_GetNumberOfEmptySlots(t_FDO_DeviceData* fdoData, ULONG ulOutputLen, IRP* pIRP);
NTSTATUS protocol_bus_GetDeviceCreateProcID(t_BusEnumProcIDReq* pReq, t_FDO_DeviceData* pFDO_Data, ULONG ulOutputLen, IRP* pIRP);


/////////////////////////////////////////////
//
// Defined in pnp.c
//
DRIVER_ADD_DEVICE Bus_EvtDeviceAdd;

__drv_dispatchType(IRP_MJ_PNP)		DRIVER_DISPATCH Bus_PnP;

NTSTATUS bus_FDO_PnP(__in DEVICE_OBJECT* pDeviceObject, __in IRP* pIRP, __in IO_STACK_LOCATION* pSackIRP, __in t_FDO_DeviceData* pFDO_DeviceData);
NTSTATUS Bus_StartFdo(__in t_FDO_DeviceData* pFDO_DeviceData, __in IRP* pIRP);
VOID Bus_RemoveFdo(__in t_FDO_DeviceData* pFDO_DeviceData);
DRIVER_DISPATCH Bus_SendIrpSynchronously;
IO_COMPLETION_ROUTINE Bus_CompletionRoutine;
///NTSTATUS bus_DestroyPDO(DEVICE_OBJECT* pDeviceObject, t_PDO_DeviceData* pPDO_DeviceData);
NTSTATUS bus_DestroyPDO(t_FDO_DeviceData* pFDO_DeviceData, t_PDO_DeviceData* pPDO_DeviceData);
VOID Bus_InitializePDO(__drv_in(__drv_aliasesMem) PDEVICE_OBJECT Pdo, t_FDO_DeviceData* FdoData);
NTSTATUS protocol_bus_PlugInDevice(t_BusEnumPluginReq* pBusEnumPlugin, t_FDO_DeviceData* pFDO_DeviceData, ULONG ulOutputLen, IRP* pIRP);
NTSTATUS protocol_bus_UnplugDevice(t_BusEnumUnplugReq* pBusEnumUnplug, t_FDO_DeviceData* pFDO_DeviceData, ULONG ulOutputLen, IRP* pIRP);
PCHAR DbgDeviceIDString(BUS_QUERY_ID_TYPE eBQIT);
PCHAR DbgDeviceRelationString(__in DEVICE_RELATION_TYPE eDRT);
PCHAR PnPMinorFunctionString(UCHAR byMinorFunction);

/////////////////////////////////////////////
//
// Defined in power.c
//
__drv_dispatchType(IRP_MJ_POWER)	DRIVER_DISPATCH Bus_Power;

NTSTATUS bus_FDO_Wakeup(t_FDO_DeviceData* pFDO_DeviceData, IRP* pIRP);
NTSTATUS bus_PDO_Wakeup(t_PDO_DeviceData* pPDO_DeviceData, IRP* pIRP);
PCHAR powerMinorFunc2str(UCHAR minorFuncCode);
PCHAR sysPowerState2str(__in SYSTEM_POWER_STATE eSysPowStat);
PCHAR devPowerState2str(__in DEVICE_POWER_STATE eDevPowStat);

/////////////////////////////////////////////
//
// Defined in buspdo.c
//
NTSTATUS Bus_PDO_PnP(__in DEVICE_OBJECT* pDeviceObject, __in IRP* pIRP, __in PIO_STACK_LOCATION pIRPStack, __in t_PDO_DeviceData* pPDO_DeviceData);
NTSTATUS bus_PDO_QueryDeviceCaps(__in t_PDO_DeviceData* pPDO_DeviceData, __in IRP* pIRP);
NTSTATUS bus_PDO_QueryDeviceId(__in t_PDO_DeviceData* pPDO_DeviceData, __in IRP* pIRP);
NTSTATUS bus_PDO_QueryDeviceText(__in t_PDO_DeviceData* pPDO_DeviceData, __in IRP* pIRP);
NTSTATUS bus_PDO_QueryResources(__in t_PDO_DeviceData* pPDO_DeviceData, __in IRP* pIRP);
NTSTATUS bus_PDO_QueryResourceRequirements(__in t_PDO_DeviceData* pPDO_DeviceData, __in IRP* pIRP);
NTSTATUS bus_PDO_QueryDeviceRelations(__in t_PDO_DeviceData* pPDO_DeviceData, __in IRP* pIRP);
NTSTATUS bus_PDO_QueryBusInformation(__in t_PDO_DeviceData* pPDO_DeviceData, __in IRP* pIRP);
NTSTATUS bus_GetDeviceCapabilities(__in DEVICE_OBJECT* pDeviceObject, __out DEVICE_CAPABILITIES* pDeviceCapabilities);

// USBDI_V3
NTSTATUS USB_BUSIFFN CB_QueryBusTimeEx(__in PVOID pvBusContext, __out PULONG puHighSpeedFrameCounter);
NTSTATUS USB_BUSIFFN CB_QueryControllerType(_In_opt_ PVOID pvBusContext, _Out_opt_ PULONG puHcdiOptionFlags, _Out_opt_ PUSHORT pusPciVendorId, _Out_opt_ PUSHORT pusPciDeviceId, _Out_opt_ PUCHAR puPciClass, _Out_opt_ PUCHAR puPciSubClass, _Out_opt_ PUCHAR puPciRevisionId, _Out_opt_ PUCHAR puPciProgIf);

// USBDI_V2
NTSTATUS USB_BUSIFFN CB_EnumLogEntry(__in PVOID pvBusContext, __in ULONG puDriverTag, __in ULONG ulEnumTag, __in ULONG ulP1, __in ULONG ulP2);

// USBDI_V1
BOOLEAN  USB_BUSIFFN CB_IsDeviceHighSpeed(__in PVOID pvBusContext);

// USB_DIV0
VOID CB_InterfaceReference(__in PVOID pvContext);
VOID CB_InterfaceDereference(__in PVOID pvContext);
NTSTATUS USB_BUSIFFN CB_QueryBusInformation(__in PVOID pvBusContext, __in ULONG ulLevel, __inout PVOID pvBusInformationBuffer, __inout PULONG pulBusInformationBufferLength, __out PULONG pulBusInformationActualLength);
NTSTATUS USB_BUSIFFN CB_SubmitIsoOutUrb(__in PVOID pvBusContext, __in URB* pURB);
NTSTATUS USB_BUSIFFN CB_QueryBusTime(__in PVOID pvBusContext, __inout PULONG puCurrentUsbFrame);
VOID     USB_BUSIFFN CB_GetUSBDIVersion(__in PVOID pvBusContext, __inout USBD_VERSION_INFORMATION* pUSBD_VersionInformation, __inout PULONG pulHcdCapabilities);

NTSTATUS bus_PDO_QueryInterface(__in t_PDO_DeviceData* pPDO_DeviceData, __in IRP* pIRP);


// PROTOCOL
NTSTATUS IOCTL_ProtocolParse(PVOID pvIrpResponse, ULONG ulInputLen, ULONG ulOutputLen, t_FDO_DeviceData* pFDO_DeviceData, /*t_Cmn_DeviceData* pCommonDeviceData,*/ IRP* pIRP);

