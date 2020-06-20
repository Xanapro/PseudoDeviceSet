#include <IoCtrl.h>


#include "./busenum.h"
#include "./ntmiscdbg.h"

#include <hidport.h>

UNICODE_STRING g_uniRegistryPath;

// sa: https://community.osr.com/discussion/195247/use-of-paged-code
// sa: https://docs.microsoft.com/en-us/windows-hardware/drivers/kernel/different-ways-of-handling-irps-cheat-sheet


// sa: https://msdn.microsoft.com/en-us/windows/desktop/ff556431?f=255&MSPPError=-2147217396ref
// sa: [Windows Internals (Developer Reference) (English Edition)]
NPAGED_LOOKASIDE_LIST g_LookAside;	// TLB


#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, Bus_DriverUnload)
#pragma alloc_text(PAGE, Bus_Create)
#pragma alloc_text(PAGE, Bus_CreateClose)
#pragma alloc_text(PAGE, Bus_CleanUp)
////
#pragma alloc_text(PAGE, Bus_DispatchSystemControl)
#pragma alloc_text(PAGE, Bus_IoCtl)
#pragma alloc_text(PAGE, Bus_Internal_IoCtl)
#endif//~defined(ALLOC_PRAGMA)



// see also: https://www.usb.org/sites/default/files/documents/hut1_12v2.pdf
//		P26.
//  sa: https://www.usb.org/sites/default/files/hutrr39b_0.pdf

//	sa: https://www.beyondlogic.org/usbnutshell/usb5.shtml
//	sa: http://mcn.oops.jp/wiki/index.php?USB%2FDescriptor
//	sa: https://discussions.apple.com/thread/6474393
//	sa: https://gist.github.com/ToadKing/b883a8ccfa26adcc6ba9905e75aeb4f2
//
//	sa: https://books.google.co.jp/books?id=cb6jDwAAQBAJ&pg=PT225&lpg=PT225&dq=ExAcquireFastMutex&source=bl&ots=WsFSjcbRsB&sig=ACfU3U276zkRjXHSi38u3coPiO3rewhOOw&hl=ja&sa=X&ved=2ahUKEwjgkLXblv7nAhWOHHAKHVKTC1oQ6AEwBHoECAsQAQ#v=onepage&q=ExAcquireFastMutex&f=false

typedef UCHAR HID_REPORT_DESCRIPTOR, *PHID_REPORT_DESCRIPTOR;
UCHAR byaDescriptorData[] = {
# define UD_IF_SUB_VENDOR_SPEC	(0xFF)	// Vendor specific
# define UD_IF_SUB_CLASS		(0x5D)	// bInterfaceSubClass
# define UD_COUNTRY_CODE		(0x00)	// bCountryCode. default 0

	
	/////////////////////////////////
	// GET_DESCRIPTOR (CONFIGURATION)
	0x09,		// bLength
	0x02,		// bDescriptorType: (CONFIGURATION 2)
	0x99, 0x00,	// !!! wTotalLength: 153 byte !!!
	/**********
	0x9E, 0x00,	// !!! wTotalLength: 153 byte !!!
	*************/
	0x04,		// bNumInterface: Interface Num of 4
	0x01,		// bConfigurationValue: shared config
	0x00,		// iConfiguration: config string desc idx.
	0xA0,		// bmAttributes: Remote wake up
	0x32, /*0xFA*/		// bMaxPower: DC Ampere * 0.5. virtual device 100mA
	// 9byte
	/////////////////////////////////

	//////////////////////////////////
	// INTERFACE Descriptor
	0x09,	0x04,	// Usage(JoyStick), bLength, bDescriptorType: Interface 4
	0x00,		// bInterfaceNumber: default 0
	0x00,		// bAlternateSetting: default 0
	0x02,		// bNumEndpoints: 2. but exclude Exnpoint0
	UD_IF_SUB_VENDOR_SPEC,	// bInterfaceClass: Vendor specific
	UD_IF_SUB_CLASS,		// bInterfaceSubClass: 
	0x01,		// bInterfaceProtocol
	0x00,		// iInterface (String Index). NO CHUNK
	// 9byte
	////////////////////////////////////

	///////////////////////////////////
	// Interface Association Descriptor
	///////////////////////////////////

	////////////////////////////////////
	// GET_DESCRIPTOR (STRING) IOCTL_HID_GET_DEVICE_DESCRIPTOR
	// class_descriptor hid_descriptor
	0x11,		// bLength
	0x21,		// bDescriptorType (HID)
	0x01, 0x10,	// BCD HID 0x0110. ver 1.10	//0x00, 0x01,  //   bcdHID 1.00
	UD_COUNTRY_CODE,		// bCountryCode: 0
	0x25,		// !!! bNumDescriptors !!!
	/***************************************/
	0x81,		//   bDescriptorType[0] (Unknown 0x81)
	//////
	////
	//
	//!!! SEEALSO: PseudoControllerBusDevice.h: sizeof (t_XIFeedback) !!!
	///0x14, 0x00,	//   wDescriptorLength[0] 20
	//0x18, 0x00,	//   wDescriptorLength[0] 24
	0x20, 0x00,		//	wDescriptorLength[0] 32
	//
	////
	/////
	0x00,		//   bDescriptorType[1] (Unknown 0x00)
	0x00, 0x00,	//   wDescriptorLength[1] 0
	0x13,		//   bDescriptorType[2] (Unknown 0x13)
	0x01, 0x08,	//   wDescriptorLength[2] 2049
	0x00,		//   bDescriptorType[3] (Unknown 0x00)
	0x00,
	/**********************************/


	//////////////////////////////////
	//
	0x07,		//   bLength
	0x05,		//   bDescriptorType (Endpoint)
	0x81,		//   bEndpointAddress (IN/D2H)
	0x03,		//   bmAttributes (Interrupt)
	0x20, 0x00,	//   wMaxPacketSize 32
	0x04,		//   bInterval 4 (unit depends on device speed)
	//
	0x07,		//   bLength
	0x05,		//   bDescriptorType (Endpoint)
	0x01,		//   bEndpointAddress (OUT/H2D)
	0x03,		//   bmAttributes (Interrupt)
	0x20, 0x00,	//   wMaxPacketSize 32
	0x08,		//   bInterval 8 (unit depends on device speed)
	//
	////////////////////////////////

	//////////////////
	// HID Descriptor?
	//////////////////
	///////////////////////////////
	/*******
	/// ADD
	0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
	0x15, 0x00,        // Logical Minimum (0)
	DAME
	*******/
	
	////////////////////////////////
	0x09,		//   bLength
	0x04,		//   bDescriptorType (Interface)
	0x01,		//   bInterfaceNumber 1
	0x00,		//   bAlternateSetting
	0x04,		//   bNumEndpoints 4
	0xFF,		//   bInterfaceClass
	UD_IF_SUB_CLASS,	//   bInterfaceSubClass
	0x03,		//   bInterfaceProtocol
	0x00,		//   iInterface (String Index)
	//
	////////////////////////////////

	////////////////////////////////
	0x1B,		//   bLength
	0x21,		//   bDescriptorType (HID)
	0x00, 0x01,	//   bcdHID 1.00
	UD_COUNTRY_CODE,		//   bCountryCode. default 0
	0x01,		//   bNumDescriptors
	0x82,		//   bDescriptorType[0] (Unknown 0x82)
	0x40, 0x01,	//   wDescriptorLength[0] 320
	0x02, 0x20, 0x16, 0x83, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x16, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	//
	///////////////////////////////////////////////////

	/////////////////////////////////////////////////
	0x07,		//   bLength
	0x05,		//   bDescriptorType (Endpoint)
	0x82,		//   bEndpointAddress (IN/D2H)
	0x03,		//   bmAttributes (Interrupt)
	0x20, 0x00,  //   wMaxPacketSize 32
	0x02,		//   bInterval 2 (unit depends on device speed)
	//
	0x07,		//   bLength
	0x05,		//   bDescriptorType (Endpoint)
	0x02,		//   bEndpointAddress (OUT/H2D)
	0x03,		//   bmAttributes (Interrupt)
	0x20, 0x00,  //   wMaxPacketSize 32
	0x04,		//   bInterval 4 (unit depends on device speed)
	//
	0x07,		//   bLength
	0x05,		//   bDescriptorType (Endpoint)
	0x83,		//   bEndpointAddress (IN/D2H)
	0x03,		//   bmAttributes (Interrupt)
	0x20, 0x00,  //   wMaxPacketSize 32
	0x40,		//   bInterval 64 (unit depends on device speed)
	//
	0x07,		//   bLength
	0x05,		//   bDescriptorType (Endpoint)
	0x03,		//   bEndpointAddress (OUT/H2D)
	0x03,		//   bmAttributes (Interrupt)
	0x20, 0x00,  //   wMaxPacketSize 32
	0x10,		//   bInterval 16 (unit depends on device speed)
	//
	///////////////////////////////////////////////////

	0x09,		//   bLength
	0x04,		//   bDescriptorType (Interface)
	0x02,		//   bInterfaceNumber 2
	0x00,		//   bAlternateSetting
	0x01,		//   bNumEndpoints 1
	0xFF,		//   bInterfaceClass
	UD_IF_SUB_CLASS,		//   bInterfaceSubClass
	0x02,		//   bInterfaceProtocol
	0x00,		//   iInterface (String Index)

	0x09,		//   bLength
	0x21,		//   bDescriptorType (HID)
	0x00, 0x01,  //   bcdHID 1.00
	UD_COUNTRY_CODE,	// bCountryCode
	0x22,		//   bNumDescriptors
	0x84,		//   bDescriptorType[0] (Unknown 0x84)
	0x07, 0x00,  //   wDescriptorLength[0] 7

	0x07,		//   bLength
	0x05,		//   bDescriptorType (Endpoint)
	0x84,		//   bEndpointAddress (IN/D2H)
	0x03,		//   bmAttributes (Interrupt)
	0x20, 0x00,  //   wMaxPacketSize 32
	0x10,		//   bInterval 16 (unit depends on device speed)

	0x09,		//   bLength
	0x04,		//   bDescriptorType (Interface)
	0x03,		//   bInterfaceNumber 3
	0x00,		//   bAlternateSetting
	0x00,		//   bNumEndpoints 0
	0xFF,		//   bInterfaceClass
	0xFD,		//   bInterfaceSubClass
	0x13,		//   bInterfaceProtocol
	0x04,		//   iInterface (String Index)

	0x06,		//   bLength
	0x41,		//   bDescriptorType (Unknown)
	0x00, 0x01, 0x01, 0x03,
	/****************
	0xC0,              // End Collection
	*************/
};


NTSTATUS DriverEntry(__in DRIVER_OBJECT* pDriverObject, __in PUNICODE_STRING pucRegistryPath)
{
	///DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_TRACE_LEVEL, "+++ START: %s, %d\n", __FILE__, __LINE__);
	fstr("+++ START: %s, %d\n", __FILE__, __LINE__);
	fstr("+++ CALLING.\n");

	ExInitializeNPagedLookasideList(&g_LookAside, NULL, NULL, 0, sizeof (t_Pending_IRP), K_BUSENUM_POOL_TAG, 0);	// alloc TLB
	g_uniRegistryPath.MaximumLength	= pucRegistryPath->Length + sizeof (UNICODE_NULL);
	g_uniRegistryPath.Length		= pucRegistryPath->Length;
	g_uniRegistryPath.Buffer		= ExAllocatePoolWithTag(PagedPool, g_uniRegistryPath.MaximumLength, K_BUSENUM_POOL_TAG);

	if (NULL == g_uniRegistryPath.Buffer) {
		return (STATUS_INSUFFICIENT_RESOURCES);
	}

	RtlCopyUnicodeString(&g_uniRegistryPath, pucRegistryPath);
	fstr("+++ RegistoryPath: %wZ\n", pucRegistryPath);

	pDriverObject->DriverExtension->AddDevice			= Bus_EvtDeviceAdd;	// Driver INSTALL
	///pDriverObject->DriverStart					= Bus_DriverLoad;
	pDriverObject->DriverUnload							= Bus_DriverUnload;	// Driver UNINSTALL
	pDriverObject->MajorFunction[IRP_MJ_CREATE]			= Bus_Create;//Bus_CreateClose;
	pDriverObject->MajorFunction[IRP_MJ_CLOSE]			= Bus_CreateClose;
	pDriverObject->MajorFunction[IRP_MJ_PNP]			= Bus_PnP;
	pDriverObject->MajorFunction[IRP_MJ_POWER]			= Bus_Power;
	pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL]	= Bus_IoCtl;
	pDriverObject->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = Bus_Internal_IoCtl;
	pDriverObject->MajorFunction[IRP_MJ_SYSTEM_CONTROL]	= Bus_DispatchSystemControl;
	pDriverObject->MajorFunction[IRP_MJ_CLEANUP]		= Bus_CleanUp;


	//odumpf(byaDescriptorData, array_sizeof (byaDescriptorData), "%u", array_sizeof (byaDescriptorData));
	//odumpf(pDriverObject, sizeof (DRIVER_OBJECT), "%u", sizeof (DRIVER_OBJECT));

	return (STATUS_SUCCESS);
}


#if 0
VOID Bus_DriverLoad(__in PDRIVER_OBJECT pDriverObject, __in PUNICODE_STRING pucRegistryPath)
{
	UNREFERENCED_PARAMETER(pDriverObject);
	PAGED_CODE();
	//
	////////////////////////////////
	if (NULl == pDriverObject->DeviceObject) {
		fstr("___ NULl == pDriverObject->DeviceObject\n");
		return;
	}

	ExInitializeNPagedLookasideList(&g_LookAside, NULL, NULL, 0, sizeof (t_Pending_IRP), K_BUSENUM_POOL_TAG, 0);

	g_uniRegistryPath.MaximumLength	= pucRegistryPath->Length + sizeof (UNICODE_NULL);
	g_uniRegistryPath.Length		= pucRegistryPath->Length;
	g_uniRegistryPath.Buffer		= ExAllocatePoolWithTag(PagedPool, g_uniRegistryPath.MaximumLength, K_BUSENUM_POOL_TAG);
	fstr("+++ Driver INSTALL done. ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
	return;
}
#endif//~if0




VOID Bus_DriverUnload(__in PDRIVER_OBJECT pDriverObject)
{
	UNREFERENCED_PARAMETER(pDriverObject);
	PAGED_CODE();
	//
	////////////////////////////////
	fstr("+++ CALLING\n");

	ExDeleteNPagedLookasideList(&g_LookAside);	// dealloc TLB

	if (NULL != g_uniRegistryPath.Buffer) {
		ExFreePool(g_uniRegistryPath.Buffer);
	}

	fstr("+++ Driver UNINSTALL done. ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
	return;
}


NTSTATUS Bus_Create(DEVICE_OBJECT* pDeviceObject, IRP* pIRP)
{
	NTSTATUS	status	= STATUS_SUCCESS;
	IO_STACK_LOCATION*	pStackIRP			= NULL;
	t_FDO_DeviceData*	pFDO_DeviceData		= NULL;
	///t_Cmn_DeviceData*	pCmn_DeviceData	= NULL;
	PAGED_CODE();
	//
	//////////////////////////////////
	fstr("+++ CALLING\n");
	//////////DbgBreakPoint();

	pFDO_DeviceData = (t_FDO_DeviceData*)pDeviceObject->DeviceExtension;
	if (!pFDO_DeviceData->m_bIsFDO) {
		pIRP->IoStatus.Status = status = STATUS_INVALID_DEVICE_REQUEST;
		IoCompleteRequest(pIRP, IO_NO_INCREMENT);
		return (status);
	}

	if (pFDO_DeviceData->m_bIsFDO) {}
	if (E_DPNP_DELETED == pFDO_DeviceData->m_eDevicePnPState) {
		pIRP->IoStatus.Status = status = STATUS_NO_SUCH_DEVICE;
		IoCompleteRequest(pIRP, IO_NO_INCREMENT);
		return (status);
	}

	bus_AddRef(pFDO_DeviceData);
	pStackIRP = IoGetCurrentIrpStackLocation(pIRP);
	switch (pStackIRP->MajorFunction) {
	case IRP_MJ_CREATE:
		fstr("+++ IRP_MJ_CREATE\n");
		status = STATUS_SUCCESS;
		break;

	case IRP_MJ_CLOSE:
		fstr("+++ IRP_MJ_CLOSE\n");
		status = STATUS_SUCCESS;
		break;

	default:
		status = STATUS_INVALID_DEVICE_REQUEST;
		break;
	}//~htiws

	pIRP->IoStatus.Information	= 0;
	pIRP->IoStatus.Status		= status;

	IoCompleteRequest(pIRP, IO_NO_INCREMENT);
	bus_ReleaseRef(pFDO_DeviceData);

	return (status);
}

#if 0
IO_COMPLETION_ROUTINE syncCompletion;
NTSTATUS syncCompletion(IN DEVICE_OBJECT* pDevObj, IN IRP* pIRP, IN PVOID pContext)
{
	UNREFERENCED_PARAMETER(pDevObj);
	UNREFERENCED_PARAMETER(pIRP);

	KEVENT* pkeSync = (PKEVENT)pContext;
	_Analysis_assume_(NULL != pkeSync);

	KeSetEvent(pkeSync, IO_NO_INCREMENT, FALSE);
	return (STATUS_MORE_PROCESSING_REQUIRED);
}
#endif//~

NTSTATUS Bus_CreateClose(DEVICE_OBJECT* pDeviceObject, IRP* pIRP)
{
	NTSTATUS	status	= STATUS_SUCCESS;
	IO_STACK_LOCATION*	pStackIRP			= NULL;
	t_FDO_DeviceData*	pFDO_DeviceData		= NULL;
	PAGED_CODE();
	//
	//////////////////////////////////
	fstr("+++ CALLING\n");

	pFDO_DeviceData = (t_FDO_DeviceData*)pDeviceObject->DeviceExtension;
	if (!pFDO_DeviceData->m_bIsFDO) {
		pIRP->IoStatus.Status = status = STATUS_INVALID_DEVICE_REQUEST;
		IoCompleteRequest(pIRP, IO_NO_INCREMENT);
		return (status);
	}

	if (E_DPNP_DELETED == pFDO_DeviceData->m_eDevicePnPState) {
		pIRP->IoStatus.Status = status = STATUS_NO_SUCH_DEVICE;
		IoCompleteRequest(pIRP, IO_NO_INCREMENT);
		return (status);
	}

	bus_AddRef(pFDO_DeviceData);
	pStackIRP = IoGetCurrentIrpStackLocation(pIRP);
	switch (pStackIRP->MajorFunction) {
	case IRP_MJ_CREATE:
		fstr("+++ IRP_MJ_CREATE\n");
		status = STATUS_SUCCESS;
		break;

	case IRP_MJ_CLOSE:
		fstr("+++ IRP_MJ_CLOSE\n");
		status = STATUS_SUCCESS;
		break;

	default:
		status = STATUS_INVALID_DEVICE_REQUEST;
		break;
	}//~hctiws

	pIRP->IoStatus.Information	= 0;
	pIRP->IoStatus.Status		= status;

	IoCompleteRequest(pIRP, IO_NO_INCREMENT);
	bus_ReleaseRef(pFDO_DeviceData);

	return (status);
}

NTSTATUS Bus_CleanUp(DEVICE_OBJECT* pDeviceObject, IRP* pIRP)
{
	NTSTATUS	status = STATUS_SUCCESS;
	DWORD		dwCurPID	= 0;
	DWORD		dwCreatePID	= 0;
	BOOLEAN		bDetected	= FALSE;
	t_PDO_DeviceData*	pPDO_DeviceData	= NULL;
	t_FDO_DeviceData*	pFDO_DeviceData	= NULL;
	LIST_ENTRY*			pEnt			= NULL;
	PAGED_CODE();
	//
	////////////////////////////
	fstr("+++ CALLING\n");

	pFDO_DeviceData = (t_FDO_DeviceData*)pDeviceObject->DeviceExtension;
	if (!pFDO_DeviceData->m_bIsFDO) {
		// Complete Request
		pIRP->IoStatus.Information	= 0;
		pIRP->IoStatus.Status		= status;
		IoCompleteRequest(pIRP, IO_NO_INCREMENT);
		return (status);
	}

	pFDO_DeviceData = (t_FDO_DeviceData*)pDeviceObject->DeviceExtension;

ExAcquireFastMutex(&pFDO_DeviceData->m_Mutex);
	dwCurPID = (DWORD)(DWORD_PTR)PsGetCurrentProcessId();
	fstr("+++ app_pid: %u\n", dwCurPID);

	for (pEnt = pFDO_DeviceData->m_lstentListOfPDOs.Flink; pEnt != &pFDO_DeviceData->m_lstentListOfPDOs; pEnt = pEnt->Flink) {
		// extract PID
		pPDO_DeviceData = CONTAINING_RECORD(pEnt, t_PDO_DeviceData, Link);
		dwCreatePID = pPDO_DeviceData->m_dwCallingPID;

		// same pid?
		if (dwCreatePID == dwCurPID) {
			fstr("#+# Plugged out %d\n", pPDO_DeviceData->m_ulPadIdx);
			pPDO_DeviceData->m_dwCallingPID = 0x00000000;
			memset(pPDO_DeviceData->m_byIndividualCode, 0, array_sizeof (pPDO_DeviceData->m_byIndividualCode));
			pPDO_DeviceData->m_bIsPresent = FALSE;
			bDetected = TRUE;		// remove mark
			break;
		}//~
	}//~rof
ExReleaseFastMutex(&pFDO_DeviceData->m_Mutex);
	
	if (bDetected) {
		IoInvalidateDeviceRelations(pFDO_DeviceData->m_pUnderlyingPDO, BusRelations);
	}

	// Complete Request
	pIRP->IoStatus.Information	= 0;
	pIRP->IoStatus.Status		= status;
	IoCompleteRequest(pIRP, IO_NO_INCREMENT);

	return (status);
}

NTSTATUS Bus_DispatchSystemControl(DEVICE_OBJECT* pDeviceObject, IRP* pIRP)
{
	// sa: https://docs.microsoft.com/en-us/windows-hardware/drivers/ifs/example--passing-the-irp-down-without-setting-a-completion-routine
	NTSTATUS			status	= STATUS_SUCCESS;
	t_FDO_DeviceData*	pFDO_DeviceData	= NULL;
	PAGED_CODE();
	//
	////////////////////////////////
	fstr("+++ CALLING\n");

	pFDO_DeviceData = (t_FDO_DeviceData*)pDeviceObject->DeviceExtension;

	IoSkipCurrentIrpStackLocation(pIRP);
	status = IoCallDriver(pFDO_DeviceData->m_pNextLowerDriver, pIRP);	// WdfDeviceWdmDispatchPreprocessedIrp(pFDO_DeviceData->m_pNextLowerDriver, pIRP)

	return (status);
}


///////////////////////////////////////////////////
//
NTSTATUS IOCTL_ProtocolParse(PVOID pvIrpResponse, ULONG ulInputLen, ULONG ulOutputLen, t_FDO_DeviceData* pFDO_DeviceData, /*t_Cmn_DeviceData* pCmn_DeviceData,*/ IRP* pIRP)
{
	t_ProtocolHdr* pHdr = NULL;
	BYTE* pbyActual = NULL;
	PVOID pvActual	= NULL;

	NTSTATUS status = STATUS_SUCCESS;
	if (sizeof (t_ProtocolHdr) > ulInputLen) {
		return (STATUS_INSUFFICIENT_RESOURCES);
	}

	pHdr = (t_ProtocolHdr*)pvIrpResponse;
	pbyActual	= (BYTE*)((t_ProtocolHdr*)(pHdr) + 1);
	pvActual	= (PVOID)((t_ProtocolHdr*)(pHdr) + 1);

	switch (pHdr->byProtocolIdx) {
	case E_PROTO_PLUGIN:	
	{
		if (sizeof (t_BusEnumPluginReq) != pHdr->byChunkSize && pHdr->byChunkSize != ulInputLen - sizeof (t_ProtocolHdr)) {
			TEXEC(5, fstr("___ INVALID_SIZE!!! protoIdx:%s(0x%02X), required:0x%08X(%d), chunk:0x%08X(%d)\n", protoid2human(pHdr->byProtocolIdx), pHdr->byProtocolIdx, sizeof (t_BusEnumPluginReq), sizeof (t_BusEnumPluginReq), pHdr->byChunkSize, pHdr->byChunkSize));
			return (STATUS_INSUFFICIENT_RESOURCES);
		}
		t_BusEnumPluginReq*	pReq = (t_BusEnumPluginReq*)pbyActual;
		status = protocol_bus_PlugInDevice(pReq, pFDO_DeviceData, ulOutputLen, pIRP);
	}
	break;
	case E_PROTO_UNPLUG:
	{
		if (sizeof (t_BusEnumUnplugReq) != pHdr->byChunkSize && pHdr->byChunkSize != ulInputLen - sizeof (t_ProtocolHdr)) {
			TEXEC(5, fstr("___ INVALID_SIZE!!! protoIdx:%s(0x%02X), required:0x%08X(%d), chunk:0x%08X(%d)\n", protoid2human(pHdr->byProtocolIdx), pHdr->byProtocolIdx, sizeof (t_BusEnumUnplugReq), sizeof (t_BusEnumUnplugReq), pHdr->byChunkSize, pHdr->byChunkSize));
			return (STATUS_INSUFFICIENT_RESOURCES);
		}
		t_BusEnumUnplugReq*	pReq	= (t_BusEnumUnplugReq*)pbyActual;
		status = protocol_bus_UnplugDevice(pReq, pFDO_DeviceData, ulOutputLen, pIRP);
	}
	break;
	case E_PROTO_REPORT:
	{
		if (sizeof (t_BusEnumReportReq) != pHdr->byChunkSize && pHdr->byChunkSize != ulInputLen - sizeof (t_ProtocolHdr)) {
			TEXEC(5, fstr("___ __________________________________________________________________________ INVALID_SIZE!!! protoIdx:%s(0x%02X), required:0x%08X(%d), chunk:0x%08X(%d)\n", protoid2human(pHdr->byProtocolIdx), pHdr->byProtocolIdx, sizeof (t_BusEnumReportReq), sizeof (t_BusEnumReportReq), pHdr->byChunkSize, pHdr->byChunkSize));
			return (STATUS_INSUFFICIENT_RESOURCES);
		}
		t_BusEnumReportReq*	pBusEnumReportReq = (t_BusEnumReportReq*)pbyActual;
		status = protocol_bus_ReportDevice(pBusEnumReportReq, pFDO_DeviceData, ulOutputLen, pIRP);
	}
	break;
	case E_PROTO_IS_PLUGED:
	{
		if (sizeof (BYTE) != pHdr->byChunkSize && pHdr->byChunkSize != ulInputLen - sizeof (t_ProtocolHdr)) {
			TEXEC(5, fstr("___ INVALID_SIZE!!! protoIdx:%s(0x%02X), required:0x%08X(%d), chunk:0x%08X(%d)\n", protoid2human(pHdr->byProtocolIdx), pHdr->byProtocolIdx, sizeof (t_BusEnumReportReq), sizeof (t_BusEnumReportReq), pHdr->byChunkSize, pHdr->byChunkSize));
			return (STATUS_INSUFFICIENT_RESOURCES);
		}
		t_BusEnumIsPlugedReq*	pReq = (t_BusEnumIsPlugedReq*)pbyActual;
		status = protocol_bus_IsDevicePluggedIn(pReq, pFDO_DeviceData, ulOutputLen, pIRP);
	}
	break;
	case E_PROTO_PROC_ID:
	{
		if (sizeof (BYTE) != pHdr->byChunkSize && pHdr->byChunkSize != ulInputLen - sizeof (t_ProtocolHdr)) {
			TEXEC(5, fstr("___ INVALID_SIZE!!! protoIdx:%s(0x%02X), required:0x%08X(%d), chunk:0x%08X(%d)\n", protoid2human(pHdr->byProtocolIdx), pHdr->byProtocolIdx, sizeof (t_BusEnumReportReq), sizeof (t_BusEnumReportReq), pHdr->byChunkSize, pHdr->byChunkSize));
			return (STATUS_INSUFFICIENT_RESOURCES);
		}
		t_BusEnumProcIDReq* pReq = (t_BusEnumProcIDReq*)pvActual;
		status = protocol_bus_GetDeviceCreateProcID(pReq, pFDO_DeviceData, ulOutputLen, pIRP);
		if (!NT_SUCCESS(status)) {
			pIRP->IoStatus.Information = sizeof (ULONG);
			if (ulOutputLen < pIRP->IoStatus.Information) {
				TEXEC(5, fstr("___ [%s(%d)] OUT OF RESPONSE BUFFER SIZE!!! %u < %u\n", protoid2human(pHdr->byProtocolIdx), pHdr->byProtocolIdx, ulOutputLen, pIRP->IoStatus.Information));
				return (STATUS_INSUFFICIENT_RESOURCES);
			}
		}//~fi
	}
	break;
	case E_PROTO_FIND_EMPTY_SLOT:
	{
		if (/*sizeof (BYTE) != pHdr->byChunkSize &&*/ pHdr->byChunkSize != ulInputLen - sizeof (t_ProtocolHdr)) {
			TEXEC(5, fstr("___ INVALID_SIZE!!! protoIdx:%s(0x%02X), required:0x%08X(%d), chunk:0x%08X(%d)\n", protoid2human(pHdr->byProtocolIdx), pHdr->byProtocolIdx, 0, 0, pHdr->byChunkSize, pHdr->byChunkSize));
			return (STATUS_INSUFFICIENT_RESOURCES);
		}
		status = protocol_bus_GetNumberOfEmptySlots(pFDO_DeviceData, ulOutputLen, pIRP);
		if (!NT_SUCCESS(status)) {
			// ERR?
			pIRP->IoStatus.Information = sizeof (UCHAR);
			if (ulOutputLen < pIRP->IoStatus.Information) {
				TEXEC(5, fstr("___ [%s(%d)] OUT OF RESPONSE BUFFER SIZE!!! %u < %u\n", protoid2human(pHdr->byProtocolIdx), pHdr->byProtocolIdx, ulOutputLen, pIRP->IoStatus.Information));
				return (STATUS_INSUFFICIENT_RESOURCES);
			}
		}//~fi
	}
	break;
	case E_PROTO_VER:
	{
		if (/*sizeof (DWORD) != pHdr->byChunkSize &&*/ pHdr->byChunkSize != ulInputLen - sizeof (t_ProtocolHdr)) {
			TEXEC(5, fstr("___ INVALID_SIZE!!! protoIdx:%s(0x%02X), required:0x%08X(%d), chunk:0x%08X(%d)\n", protoid2human(pHdr->byProtocolIdx), pHdr->byProtocolIdx, 0, 0, pHdr->byChunkSize, pHdr->byChunkSize));
			return (STATUS_INSUFFICIENT_RESOURCES);
		}
		t_BusEnumVersionRes res;
		res.dwVersion = BUS_VERSION;
		res.pad0 = 0xF0E0D0C0;
		status = irpcopy(pIRP, &res, sizeof (t_BusEnumVersionRes), ulOutputLen);
	}
	break;
	default:
		TEXEC(5, fstr("___ UNKNOWN PROTOCOL: protoIdx: 0x02X, chunk: 0x%02X\n", pHdr->byProtocolIdx, pHdr->byChunkSize));
		status = STATUS_NOT_IMPLEMENTED;
		break;
	}//~hctiws
	return (status);
}



// 
// 	
// App	DeviceIoControl()						reply DeviceIoControl(,,,, OUT lpOutBuffer, OUT nOutBufferSize)
// Drv			this->Bus_IoCtl()	memcpy(IRP.AssociatedIrp.SystemBuffer, IRP.IoStatus.Information )
//
NTSTATUS Bus_IoCtl(DEVICE_OBJECT* pDeviceObject, IRP* pIRP)
{
	NTSTATUS	status	= STATUS_SUCCESS;
	ULONG		ulInputLen			= 0;
	ULONG		ulOutputLen			= 0;
	PVOID		pvIrpResponse		= NULL;
	IO_STACK_LOCATION*	pStackIRP	= NULL;
	t_FDO_DeviceData*	pFDO_DeviceData		= NULL;
	///t_Cmn_DeviceData*	pCmn_DeviceData	= NULL;
///	PAGED_CODE();
	//
	////////////////////////////////////////

	///pCmn_DeviceData = (t_Cmn_DeviceData*)pDeviceObject->DeviceExtension;
	pFDO_DeviceData = (t_FDO_DeviceData*)pDeviceObject->DeviceExtension;
	if (!pFDO_DeviceData->m_bIsFDO) {
		pIRP->IoStatus.Status = status = STATUS_INVALID_DEVICE_REQUEST;
		IoCompleteRequest(pIRP, IO_NO_INCREMENT);
		return (status);
	}

	if (E_DPNP_DELETED == pFDO_DeviceData->m_eDevicePnPState) {
		// not on the bus
		pIRP->IoStatus.Status = status = STATUS_NO_SUCH_DEVICE;
		IoCompleteRequest(pIRP, IO_NO_INCREMENT);
		return (status);
	}

	bus_AddRef(pFDO_DeviceData);
	pStackIRP = IoGetCurrentIrpStackLocation(pIRP);

	pvIrpResponse	= pIRP->AssociatedIrp.SystemBuffer;
	ulInputLen		= pStackIRP->Parameters.DeviceIoControl.InputBufferLength;
	ulOutputLen		= pStackIRP->Parameters.DeviceIoControl.OutputBufferLength;

	status = STATUS_INVALID_PARAMETER;
	pIRP->IoStatus.Information = 0;

	switch (pStackIRP->Parameters.DeviceIoControl.IoControlCode) {
	case IOCTL_BUS_PROTOCOL:
		status = IOCTL_ProtocolParse(pvIrpResponse, ulInputLen, ulOutputLen, pFDO_DeviceData, /*pCmn_DeviceData,*/ pIRP);
		break;
	default:
		break;
	}//~hctiws

	pIRP->IoStatus.Status = status;

	IoCompleteRequest(pIRP, IO_NO_INCREMENT);
	bus_ReleaseRef(pFDO_DeviceData);

	return (status);
}

void udpate_USBD_PUPE_INFO(
	USBD_PIPE_INFORMATION* p
	, USHORT MaximumPacketSize
	, UCHAR EndpointAddress
	, UCHAR Interval
	, USBD_PIPE_TYPE PipeType
	, USBD_PIPE_HANDLE PipeHandle
	, ULONG MaximumTransferSize
	, ULONG PipeFlags
) {
	p->MaximumPacketSize	= MaximumPacketSize;
	p->EndpointAddress		= EndpointAddress;
	p->Interval				= Interval;
	p->PipeType				= PipeType;
	p->PipeHandle			= PipeHandle;
	p->MaximumTransferSize	= MaximumTransferSize;
	p->PipeFlags			= PipeFlags;
}



NTSTATUS Bus_Internal_IoCtl(DEVICE_OBJECT* pDeviceObject, IRP* pIRP)
{
	NTSTATUS	status		= STATUS_SUCCESS;
	ULONG		ulInputLen	= 0;
	IO_STACK_LOCATION*	pStackIRP			= NULL;
	t_PDO_DeviceData*	pPDO_DeviceData		= NULL;
///	PAGED_CODE();
	//
	//////////////////////////////////////

	pPDO_DeviceData = (t_PDO_DeviceData*)pDeviceObject->DeviceExtension;
	pStackIRP			= IoGetCurrentIrpStackLocation(pIRP);
	if (pPDO_DeviceData->m_bIsFDO) {
		pIRP->IoStatus.Status = status = STATUS_INVALID_DEVICE_REQUEST;
		IoCompleteRequest(pIRP, IO_NO_INCREMENT);
		return (status);
	}

	if (FALSE == pPDO_DeviceData->m_bIsPresent) {
		// not on the bus
		pIRP->IoStatus.Status = status = STATUS_DEVICE_NOT_CONNECTED;
		IoCompleteRequest(pIRP, IO_NO_INCREMENT);
		return (status);
	}

	ulInputLen	= pStackIRP->Parameters.DeviceIoControl.InputBufferLength;
	status = STATUS_INVALID_PARAMETER;
	switch (pStackIRP->Parameters.DeviceIoControl.IoControlCode) {
	case IOCTL_INTERNAL_USB_SUBMIT_URB:
	{
		URB* pHxp = (URB*)pStackIRP->Parameters.Others.Argument1;

		switch (pHxp->UrbHeader.Function)
		{
		case URB_FUNCTION_CONTROL_TRANSFER:
		{
			status = STATUS_UNSUCCESSFUL;
		}
		break;

		// sa https://books.google.co.jp/books?id=OXOnyEBPhXcC&pg=PA463&lpg=PA463&dq=USBD_PIPE_HANDLE+USBD_TRANSFER_DIRECTION_IN&source=bl&ots=A__60NYL6v&sig=ACfU3U2e-uArU9Gr-T7GpSqQVCvWkiUbFw&hl=ja&sa=X&ved=2ahUKEwj9rZP71YDoAhUOa94KHcJPCmIQ6AEwBnoECAkQAQ#v=onepage&q=USBD_PIPE_HANDLE%20USBD_TRANSFER_DIRECTION_INUSBD_TRANSFER_DIRECTION_IN&f=false
		// requested bulk or interrupt transfer (HID input and output reports)
		case URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER:
		{
			struct _URB_BULK_OR_INTERRUPT_TRANSFER* pTransfer = &pHxp->UrbBulkOrInterruptTransfer;

			if (pPDO_DeviceData->m_eDevicePnPState != E_DPNP_STARTED) {
				status = STATUS_UNSUCCESSFUL;
			}
			else if (pTransfer->TransferFlags & USBD_TRANSFER_DIRECTION_IN) {
				KIRQL PrevIrql;	// actual types UCHAR
				UCHAR Pipe = pTransfer->PipeHandle == (USBD_PIPE_HANDLE)0xFFFF0081 ? 0x81 : 0x83;

				status = STATUS_PENDING;
				IoMarkIrpPending(pIRP);

KeAcquireSpinLock(&pPDO_DeviceData->m_lstentPendingQueueLock, &PrevIrql);
				{
					IoSetCancelRoutine(pIRP, CB_CancelIrp);

					if (pIRP->Cancel == TRUE) {
						IoSetCancelRoutine(pIRP, NULL);
		//KeReleaseSpinLock(&pPDO_DeviceData->m_lstentPendingQueueLock, PrevIrql);

						pIRP->IoStatus.Status		= STATUS_CANCELLED;
						pIRP->IoStatus.Information = 0;

						IoCompleteRequest(pIRP, IO_NO_INCREMENT);
					}
					else {
						// sa: https://www.coursehero.com/file/p1t0nt5r/struct-URBBULKORINTERRUPTTRANSFER-pTransfer-pHxp-UrbBulkOrInterruptTransfer-if/
						t_Pending_IRP* le = ExAllocateFromNPagedLookasideList(&g_LookAside);
						
						//@@@@@@@@@@@@@
						le->Irp = pIRP;
						if (Pipe == 0x81) {	//!!!
							InsertTailList(&pPDO_DeviceData->m_lstentPendingQueue, &le->Link);
						}
						else {
							InsertTailList(&pPDO_DeviceData->m_lstentHoldingQueue, &le->Link);
						}//
		//KeReleaseSpinLock(&pPDO_DeviceData->m_lstentPendingQueueLock, PrevIrql);
					}//~esle
				}
KeReleaseSpinLock(&pPDO_DeviceData->m_lstentPendingQueueLock, PrevIrql);
			}//~esle fi
			else {
				fstr("<<< URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER: Handle %p, Flags %X, Length %d\n",
					pTransfer->PipeHandle,
					pTransfer->TransferFlags,
					pTransfer->TransferBufferLength);

				status = STATUS_SUCCESS;

				if (/*sizeof (DWORD)*/K_QUADRANT_LIGHT_LEN == pTransfer->TransferBufferLength) { // !!! QuadrantLight: 3 byte
					UCHAR* pbyQuadrant = pTransfer->TransferBuffer;

					fstr("+++ QuadrantLight pbyQuadrant: %02X %02X %02X\n", pbyQuadrant[0], pbyQuadrant[1], pbyQuadrant[2]);

					// extract QuadrantLight byte to get controller slot
					if (pbyQuadrant[0] == 0x01 && pbyQuadrant[1] == 0x03) {
						switch (pbyQuadrant[2]) {
						case 0x02:	pPDO_DeviceData->m_byRingLightNo = 0;	break;
						case 0x03:	pPDO_DeviceData->m_byRingLightNo = 1;	break;
						case 0x04:	pPDO_DeviceData->m_byRingLightNo = 2;	break;
						case 0x05:	pPDO_DeviceData->m_byRingLightNo = 3;	break;
#if 0
							// wireless cntroller?
						case 0x06:	pPDO_DeviceData->m_byRingLightNo = 4;	break;
						case 0x07:	pPDO_DeviceData->m_byRingLightNo = 5;	break;
						case 0x08:	pPDO_DeviceData->m_byRingLightNo = 6;	break;
						case 0x09:	pPDO_DeviceData->m_byRingLightNo = 7;	break;
#endif//~if0
						default:	fstr("#_# out of val: RingLightNo: %u\n", pPDO_DeviceData->m_byRingLightNo);	 break;
						}//~hctiws
						fstr("+++ QuadrantLight No: %d\n", pPDO_DeviceData->m_byRingLightNo);
					}//~fi
				}//~fi

				if (pTransfer->TransferBufferLength == sizeof (DWORD64)) { // Rumble
					UCHAR* pbyQuadrant = pTransfer->TransferBuffer;

					fstr("-- Rumble pbyQuadrant: %02X %02X %02X %02X %02X %02X %02X %02X\n",
						pbyQuadrant[0],
						pbyQuadrant[1],
						pbyQuadrant[2],
						pbyQuadrant[3],
						pbyQuadrant[4],
						pbyQuadrant[5],
						pbyQuadrant[6],
						pbyQuadrant[7]);

					RtlCopyBytes(pPDO_DeviceData->m_byaRumble, pbyQuadrant, pTransfer->TransferBufferLength);

				}//~fi
			}//~esle
		}//~case URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER:
		break;

		// sa: https://docs.microsoft.com/en-us/windows-hardware/drivers/ddi/usb/ns-usb-_urb_select_configuration
		case URB_FUNCTION_SELECT_CONFIGURATION:
		{
			//	2,		4,		1,		0
			//	72,		120,	48,		24
			USBD_INTERFACE_INFORMATION* pInfo = &pHxp->UrbSelectConfiguration.Interface;
			fstr("+++ URB_FUNCTION_SELECT_CONFIGURATION: TotalLength %u\n", pHxp->UrbHeader.Length);
			
			status = STATUS_SUCCESS;
			fstr("+++ URB_FUNCTION_SELECT_CONFIGURATION: Length %u, Interface %u, Alternate %u, Pipes %u\n",
				(unsigned)pInfo->Length,
				(unsigned)pInfo->InterfaceNumber,
				(unsigned)pInfo->AlternateSetting,
				pInfo->NumberOfPipes);

			pInfo->Class = UD_IF_SUB_VENDOR_SPEC;//0xFF;
			pInfo->SubClass = UD_IF_SUB_CLASS;// 0x5D;
			pInfo->Protocol = 0x01;	// usb1

			pInfo->InterfaceHandle = (USBD_INTERFACE_HANDLE)0xFFFF0000;

			pInfo->Pipes[0].MaximumTransferSize = 0x00400000;
			pInfo->Pipes[0].MaximumPacketSize = 0x20;
			pInfo->Pipes[0].EndpointAddress = 0x81;
			pInfo->Pipes[0].Interval = 0x04;
			pInfo->Pipes[0].PipeType = 0x03;
			pInfo->Pipes[0].PipeHandle = (USBD_PIPE_HANDLE)0xFFFF0081;
			pInfo->Pipes[0].PipeFlags = 0x00;
			//udpate_USBD_PUPE_INFO(pInfo->Pipes[0], 0x00400000, 0x81, 0x04, 0x03, 0x03

			pInfo->Pipes[1].MaximumTransferSize = 0x00400000;
			pInfo->Pipes[1].MaximumPacketSize = 0x20;
			pInfo->Pipes[1].EndpointAddress = 0x01;
			pInfo->Pipes[1].Interval = 0x08;
			pInfo->Pipes[1].PipeType = 0x03;
			pInfo->Pipes[1].PipeHandle = (USBD_PIPE_HANDLE)0xFFFF0001;
			pInfo->Pipes[1].PipeFlags = 0x00;

			pInfo = (USBD_INTERFACE_INFORMATION*)((PCHAR)pInfo + pInfo->Length);

			fstr("+++ URB_FUNCTION_SELECT_CONFIGURATION: Length %u, Interface %u, Alternate %u, Pipes %u\n",
				(unsigned)pInfo->Length,
				(unsigned)pInfo->InterfaceNumber,
				(unsigned)pInfo->AlternateSetting,
				pInfo->NumberOfPipes);

			pInfo->Class = UD_IF_SUB_VENDOR_SPEC;//0xFF;
			pInfo->SubClass = UD_IF_SUB_CLASS;// 0x5D;
			pInfo->Protocol = 0x03;	// usb3

			pInfo->InterfaceHandle = (USBD_INTERFACE_HANDLE)0xFFFF0000;

			// !!!
			pInfo->Pipes[0].MaximumTransferSize = 0x00400000;
			pInfo->Pipes[0].MaximumPacketSize = 0x20;
			pInfo->Pipes[0].EndpointAddress = 0x82;
			pInfo->Pipes[0].Interval = 0x04;
			pInfo->Pipes[0].PipeType = 0x03;
			pInfo->Pipes[0].PipeHandle = (USBD_PIPE_HANDLE)0xFFFF0082;
			pInfo->Pipes[0].PipeFlags = 0x00;

			pInfo->Pipes[1].MaximumTransferSize = 0x00400000;
			pInfo->Pipes[1].MaximumPacketSize = 0x20;
			pInfo->Pipes[1].EndpointAddress = 0x02;
			pInfo->Pipes[1].Interval = 0x08;
			pInfo->Pipes[1].PipeType = 0x03;
			pInfo->Pipes[1].PipeHandle = (USBD_PIPE_HANDLE)0xFFFF0002;
			pInfo->Pipes[1].PipeFlags = 0x00;

			pInfo->Pipes[2].MaximumTransferSize = 0x00400000;
			pInfo->Pipes[2].MaximumPacketSize = 0x20;
			pInfo->Pipes[2].EndpointAddress = 0x83;
			pInfo->Pipes[2].Interval = 0x08;
			pInfo->Pipes[2].PipeType = 0x03;
			pInfo->Pipes[2].PipeHandle = (USBD_PIPE_HANDLE)0xFFFF0083;
			pInfo->Pipes[2].PipeFlags = 0x00;

			pInfo->Pipes[3].MaximumTransferSize = 0x00400000;
			pInfo->Pipes[3].MaximumPacketSize = 0x20;
			pInfo->Pipes[3].EndpointAddress = 0x03;
			pInfo->Pipes[3].Interval = 0x08;
			pInfo->Pipes[3].PipeType = 0x03;
			pInfo->Pipes[3].PipeHandle = (USBD_PIPE_HANDLE)0xFFFF0003;
			pInfo->Pipes[3].PipeFlags = 0x00;

			pInfo = (USBD_INTERFACE_INFORMATION*)((PCHAR)pInfo + pInfo->Length);
			//
			/////////////////////////////////////////////////////////////

			fstr("+++ URB_FUNCTION_SELECT_CONFIGURATION: Length %u, Interface %u, Alternate %u, Pipes %u\n",
				(unsigned)pInfo->Length,
				(unsigned)pInfo->InterfaceNumber,
				(unsigned)pInfo->AlternateSetting,
				pInfo->NumberOfPipes);

			pInfo->Class = UD_IF_SUB_VENDOR_SPEC;//0xFF;
			pInfo->SubClass = UD_IF_SUB_CLASS;// 0x5D;
			pInfo->Protocol = 0x02;	// usb2

			pInfo->InterfaceHandle = (USBD_INTERFACE_HANDLE)0xFFFF0000;

			pInfo->Pipes[0].MaximumTransferSize = 0x00400000;
			pInfo->Pipes[0].MaximumPacketSize = 0x20;
			pInfo->Pipes[0].EndpointAddress = 0x84;
			pInfo->Pipes[0].Interval = 0x04;
			pInfo->Pipes[0].PipeType = 0x03;
			pInfo->Pipes[0].PipeHandle = (USBD_PIPE_HANDLE)0xFFFF0084;
			pInfo->Pipes[0].PipeFlags = 0x00;

			pInfo = (USBD_INTERFACE_INFORMATION*)((PCHAR)pInfo + pInfo->Length);

			fstr("+++ URB_FUNCTION_SELECT_CONFIGURATION: Length %u, Interface %u, Alternate %u, Pipes %u\n",
				(unsigned)pInfo->Length,
				(unsigned)pInfo->InterfaceNumber,
				(unsigned)pInfo->AlternateSetting,
				pInfo->NumberOfPipes);

			// TERM pipe
			pInfo->Class = UD_IF_SUB_VENDOR_SPEC;//0xFF;
			pInfo->SubClass = 0xFD;		// !UD_IF_SUB_CLASS 
			pInfo->Protocol = 0x13;

			pInfo->InterfaceHandle = (USBD_INTERFACE_HANDLE)0xFFFF0000;
		}//~case URB_FUNCTION_SELECT_CONFIGURATION:
		break;

		case URB_FUNCTION_SELECT_INTERFACE:
		{
			//	4,		1
			//	120,	48

			USBD_INTERFACE_INFORMATION* pInfo = &pHxp->UrbSelectInterface.Interface;
			fstr("+++ URB_FUNCTION_SELECT_INTERFACE: Length %u, Interface %u, Alternate %u, Pipes %u\n",
				(unsigned)pInfo->Length,
				(unsigned)pInfo->InterfaceNumber,
				(unsigned)pInfo->AlternateSetting,
				pInfo->NumberOfPipes);

			fstr("+++ URB_FUNCTION_SELECT_INTERFACE: Class %u, SubClass %u, Protocol %u\n",
				(unsigned)pInfo->Class,
				(unsigned)pInfo->SubClass,
				(unsigned)pInfo->Protocol);

			if (1 == pInfo->InterfaceNumber) {
				status = STATUS_SUCCESS;

				pInfo->Class = UD_IF_SUB_VENDOR_SPEC;//0xFF;
				pInfo->SubClass = UD_IF_SUB_CLASS;// 0x5D;
				pInfo->Protocol = 0x03;
				pInfo->NumberOfPipes = 0x04;

				pInfo->InterfaceHandle = (USBD_INTERFACE_HANDLE)0xFFFF0000;

				pInfo->Pipes[0].MaximumTransferSize = 0x00400000;
				pInfo->Pipes[0].MaximumPacketSize = 0x20;
				pInfo->Pipes[0].EndpointAddress = 0x82;
				pInfo->Pipes[0].Interval = 0x04;
				pInfo->Pipes[0].PipeType = 0x03;
				pInfo->Pipes[0].PipeHandle = (USBD_PIPE_HANDLE)0xFFFF0082;
				pInfo->Pipes[0].PipeFlags = 0x00;

				pInfo->Pipes[1].MaximumTransferSize = 0x00400000;
				pInfo->Pipes[1].MaximumPacketSize = 0x20;
				pInfo->Pipes[1].EndpointAddress = 0x02;
				pInfo->Pipes[1].Interval = 0x08;
				pInfo->Pipes[1].PipeType = 0x03;
				pInfo->Pipes[1].PipeHandle = (USBD_PIPE_HANDLE)0xFFFF0002;
				pInfo->Pipes[1].PipeFlags = 0x00;

				pInfo->Pipes[2].MaximumTransferSize = 0x00400000;
				pInfo->Pipes[2].MaximumPacketSize = 0x20;
				pInfo->Pipes[2].EndpointAddress = 0x83;
				pInfo->Pipes[2].Interval = 0x08;
				pInfo->Pipes[2].PipeType = 0x03;
				pInfo->Pipes[2].PipeHandle = (USBD_PIPE_HANDLE)0xFFFF0083;
				pInfo->Pipes[2].PipeFlags = 0x00;

				pInfo->Pipes[3].MaximumTransferSize = 0x00400000;
				pInfo->Pipes[3].MaximumPacketSize = 0x20;
				pInfo->Pipes[3].EndpointAddress = 0x03;
				pInfo->Pipes[3].Interval = 0x08;
				pInfo->Pipes[3].PipeType = 0x03;
				pInfo->Pipes[3].PipeHandle = (USBD_PIPE_HANDLE)0xFFFF0003;
				pInfo->Pipes[3].PipeFlags = 0x00;
			}//~fi
			else if (2 == pInfo->InterfaceNumber) {
				status = STATUS_SUCCESS;

				pInfo->Class = UD_IF_SUB_VENDOR_SPEC;//0xFF;
				pInfo->SubClass = UD_IF_SUB_CLASS;// 0x5D;
				pInfo->Protocol = 0x02;
				pInfo->NumberOfPipes = 0x01;

				pInfo->InterfaceHandle = (USBD_INTERFACE_HANDLE)0xFFFF0000;

				pInfo->Pipes[0].MaximumTransferSize = 0x00400000;
				pInfo->Pipes[0].MaximumPacketSize = 0x20;
				pInfo->Pipes[0].EndpointAddress = 0x84;
				pInfo->Pipes[0].Interval = 0x04;
				pInfo->Pipes[0].PipeType = 0x03;
				pInfo->Pipes[0].PipeHandle = (USBD_PIPE_HANDLE)0xFFFF0084;
				pInfo->Pipes[0].PipeFlags = 0x00;
			}//~esle fi
			else {
				fstr("___ SYSTEM DESIGN ERROR? InterfaceNumber: %u\n", pInfo->InterfaceNumber);
			}
		}//~case URB_FUNCTION_SELECT_INTERFACE:
		break;

		case URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE:
		{
			fstr("+++ Descriptor: Type %u, Index %u\n", pHxp->UrbControlDescriptorRequest.DescriptorType, pHxp->UrbControlDescriptorRequest.Index);

			switch (pHxp->UrbControlDescriptorRequest.DescriptorType)
			{
			case USB_DEVICE_DESCRIPTOR_TYPE:
			{
				fstr("+++ USB_DEVICE_DESCRIPTOR_TYPE: Buffer %p, Length %u\n",
					pHxp->UrbControlDescriptorRequest.TransferBuffer,
					pHxp->UrbControlDescriptorRequest.TransferBufferLength);

				USB_DEVICE_DESCRIPTOR* pDescriptor = (USB_DEVICE_DESCRIPTOR*)pHxp->UrbControlDescriptorRequest.TransferBuffer;

				status = STATUS_SUCCESS;

				pDescriptor->bLength = 0x12;
				pDescriptor->bDescriptorType = USB_OTHER_SPEED_CONFIGURATION_DESCRIPTOR_TYPE;	//USB_DEVICE_DESCRIPTOR_TYPE;
				pDescriptor->bcdUSB = 0x0200; // USB v2.0
				pDescriptor->bDeviceClass = 0xFF;
				pDescriptor->bDeviceSubClass = 0xFF;
				pDescriptor->bDeviceProtocol = 0xFF;
				pDescriptor->bMaxPacketSize0 = 0x08;
				//!!! VENDOR/PRODCTID !!!
				///pDescriptor->idVendor = 0x16C0;	// [0x16C0]Commercial Licenses for V-USB
				///pDescriptor->idVendor = 0x045E;	// [‚O‚˜‚O‚S‚T‚d] Microsoft Corp.
				pDescriptor->idVendor = K_USB_VENDOR_ID;		// Xanapro USB Vendor ID
				pDescriptor->idProduct = 0x028E;	// ‚O‚Q‚W‚d Xbox360 Wired Controller
				///pDescriptor->idProduct = 0x27DC;	// ‚Q‚V‚c‚b Xbox360 Wireless Controller
				pDescriptor->bcdDevice = 0x0114;
				pDescriptor->iManufacturer = 0x01;
				pDescriptor->iProduct = 0x02;
				pDescriptor->iSerialNumber = 0x03;
				pDescriptor->bNumConfigurations = 0x01;
			}//~case USB_DEVICE_DESCRIPTOR_TYPE
			break;

			case USB_CONFIGURATION_DESCRIPTOR_TYPE:
			{
				fstr("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ START: %s, %d\n", __FILE__, __LINE__);
				fstr("+++ USB_CONFIGURATION_DESCRIPTOR_TYPE: Buffer %p, Length %d\n", pHxp->UrbControlDescriptorRequest.TransferBuffer, pHxp->UrbControlDescriptorRequest.TransferBufferLength);
				// sa: usbspec.h
				USB_CONFIGURATION_DESCRIPTOR* pDescriptor = (USB_CONFIGURATION_DESCRIPTOR*)pHxp->UrbControlDescriptorRequest.TransferBuffer;

				status = STATUS_SUCCESS;

				pDescriptor->bLength = 0x09;
				pDescriptor->bDescriptorType = USB_OTHER_SPEED_CONFIGURATION_DESCRIPTOR_TYPE;	// ORG USB_CONFIGURATION_DESCRIPTOR_TYPE;
				pDescriptor->wTotalLength = sizeof (byaDescriptorData);
				pDescriptor->bNumInterfaces = 0x04;
				pDescriptor->bConfigurationValue = 0x01;
				pDescriptor->iConfiguration = 0x00;
				pDescriptor->bmAttributes = 0xA0;
				pDescriptor->MaxPower = 0xFA;

				if (pHxp->UrbControlDescriptorRequest.TransferBufferLength >= sizeof (byaDescriptorData)) {
					UCHAR* Buffer = pHxp->UrbControlDescriptorRequest.TransferBuffer;
					memcpy(Buffer, byaDescriptorData, sizeof (byaDescriptorData));
				}//~fi
			}//~case USB_CONFIGURATION_DESCRIPTOR_TYPE:
			break;

			case USB_STRING_DESCRIPTOR_TYPE:
			{
				fstr("+++ USB_STRING_DESCRIPTOR_TYPE: Index %d, Buffer %p, Length %d\n",
					pHxp->UrbControlDescriptorRequest.Index,
					pHxp->UrbControlDescriptorRequest.TransferBuffer,
					pHxp->UrbControlDescriptorRequest.TransferBufferLength);

				if (pHxp->UrbControlDescriptorRequest.Index == 2) {
					PUSB_STRING_DESCRIPTOR pDescriptor = (PUSB_STRING_DESCRIPTOR)pHxp->UrbControlDescriptorRequest.TransferBuffer;
					WCHAR*	bString = L"Controller";
					USHORT  Length;

					status = STATUS_SUCCESS;

					Length = (USHORT)((wcslen(bString) + 1) * sizeof (WCHAR));

					pDescriptor->bLength = (UCHAR)(sizeof (USB_STRING_DESCRIPTOR) + Length);
					pDescriptor->bDescriptorType = USB_STRING_DESCRIPTOR_TYPE;

					if (pHxp->UrbControlDescriptorRequest.TransferBufferLength >= pDescriptor->bLength) {
						RtlStringCchPrintfW(pDescriptor->bString, Length / sizeof (WCHAR), L"%ws", bString);
					}
				}//~fi
			}//~case USB_STRING_DESCRIPTOR_TYPE:
			break;

			case USB_INTERFACE_DESCRIPTOR_TYPE:
			{
				fstr("+++ USB_INTERFACE_DESCRIPTOR_TYPE: Buffer %p, Length %d\n", pHxp->UrbControlDescriptorRequest.TransferBuffer, pHxp->UrbControlDescriptorRequest.TransferBufferLength);
			}
			break;

			case USB_ENDPOINT_DESCRIPTOR_TYPE:
			{
				fstr("+++ USB_ENDPOINT_DESCRIPTOR_TYPE: Buffer %p, Length %d\n", pHxp->UrbControlDescriptorRequest.TransferBuffer, pHxp->UrbControlDescriptorRequest.TransferBufferLength);
			}
			break;

			default:
				break;
			}//~hctiws URB_FUNCTION_GET_DESCRIPTOR
		}//~case URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE:
		break;
		}//~hctiws
	}//~case IOCTL_INTERNAL_USB_SUBMIT_URB
	break;

	case IOCTL_INTERNAL_USB_GET_PORT_STATUS:
		fstr("+++ IOCTL_INTERNAL_USB_GET_PORT_STATUS\n");
		*(unsigned long*)pStackIRP->Parameters.Others.Argument1 = USBD_PORT_ENABLED | USBD_PORT_CONNECTED;
		status = STATUS_SUCCESS;
		break;

	case IOCTL_INTERNAL_USB_RESET_PORT:
		fstr("+++ IOCTL_INTERNAL_USB_RESET_PORT\n");
		status = STATUS_SUCCESS;
		break;

	default:
		fstr("___ Unknown IOCTRL code\n");
		break;
	}//~hctiws

	pIRP->IoStatus.Information = 0;

	if (status != STATUS_PENDING) {
		pIRP->IoStatus.Status = status;
		IoCompleteRequest(pIRP, IO_NO_INCREMENT);
	}

	return (status);
}

VOID CB_CancelIrp(IN DEVICE_OBJECT* pDeviceObject, IN IRP* pIRP)
{
	KIRQL				irql;
	LIST_ENTRY*			pEnt			= NULL;
	IRP*				pIRP_Cancel		= NULL;
	t_PDO_DeviceData*	pPDO_DeviceData	= NULL;
	//
	///////////////////////////////////////
	fstr("+++ CB_CancelIrp: %p\n", pIRP);
	
	pPDO_DeviceData	= (t_PDO_DeviceData*)pDeviceObject->DeviceExtension;

IoReleaseCancelSpinLock(pIRP->CancelIrql);

KeAcquireSpinLock(&pPDO_DeviceData->m_lstentPendingQueueLock, &irql);
	// PENDING
	for (pEnt = pPDO_DeviceData->m_lstentPendingQueue.Flink; pEnt != &pPDO_DeviceData->m_lstentPendingQueue; pEnt = pEnt->Flink) {
		t_Pending_IRP*	pPendIRP = CONTAINING_RECORD(pEnt, t_Pending_IRP, Link);
		pIRP_Cancel = pPendIRP->Irp;
		if (pIRP_Cancel->Cancel && (pIRP == pIRP_Cancel)) {
			fstr("+++ m_lstentPendingQueue: %p\n", pIRP_Cancel);
				//ExAcquireFastMutex(&pPDO_DeviceData->m_Mutex);
			RemoveEntryList(pEnt);
				//ExReleaseFastMutex(&pPDO_DeviceData->m_Mutex);
			break;
		}//~fi
		pIRP_Cancel = NULL;
	}//~rof

	// HOLDING
	if (NULL == pIRP_Cancel) {
		for (pEnt = pPDO_DeviceData->m_lstentHoldingQueue.Flink; pEnt != &pPDO_DeviceData->m_lstentHoldingQueue; pEnt = pEnt->Flink) {
			t_Pending_IRP*	pPendIRP = CONTAINING_RECORD(pEnt, t_Pending_IRP, Link);
			pIRP_Cancel = pPendIRP->Irp;
			if (pIRP_Cancel->Cancel && (pIRP == pIRP_Cancel)) {
				fstr("+++ m_lstentHoldingQueue: %p\n", pIRP_Cancel);
					//ExAcquireFastMutex(&pPDO_DeviceData->m_Mutex);
				RemoveEntryList(pEnt);
					//ExReleaseFastMutex(&pPDO_DeviceData->m_Mutex);
				break;
			}//~fi
			pIRP_Cancel = NULL;
		}//~rof
	}//~fi

KeReleaseSpinLock(&pPDO_DeviceData->m_lstentPendingQueueLock, irql);
	if (NULL != pIRP_Cancel) {
		pIRP_Cancel->IoStatus.Status		= STATUS_CANCELLED;
		pIRP_Cancel->IoStatus.Information	= 0;
		IoCompleteRequest(pIRP_Cancel, IO_NO_INCREMENT);
	}
}


VOID bus_AddRef(__in t_FDO_DeviceData* pFDO_DeviceData)
{
	LONG result = InterlockedIncrement((LONG*)&pFDO_DeviceData->m_ulOutstandingIO);
	///if (1 == result)
	if (2 == result) {
		KeClearEvent(&pFDO_DeviceData->m_evtStop);
	}
	return;
}

VOID bus_ReleaseRef(__in t_FDO_DeviceData* pFDO_DeviceData)
{
	LONG result = InterlockedDecrement((LONG*)&pFDO_DeviceData->m_ulOutstandingIO);
	if (1 == result) {
		KeSetEvent(&pFDO_DeviceData->m_evtStop, IO_NO_INCREMENT, FALSE);
	}

	if (0 == result) {
		ASSERT(E_DPNP_DELETED == pFDO_DeviceData->m_eDevicePnPState);
		KeSetEvent(&pFDO_DeviceData->m_evtRemove, IO_NO_INCREMENT, FALSE);
	}
	return;
}

NTSTATUS protocol_bus_ReportDevice(t_BusEnumReportReq* pReq, t_FDO_DeviceData* pFDO_DeviceData, ULONG ulOutputLen, IRP* pIRP)
{
	UNREFERENCED_PARAMETER(ulOutputLen);
	UNREFERENCED_PARAMETER(pIRP);
	NTSTATUS			status = STATUS_SUCCESS;
	PLIST_ENTRY			entry;
	t_PDO_DeviceData*	pdoData = NULL;
	BOOLEAN				bDetect = FALSE;
	PIRP				PendingIrp = NULL;
	KIRQL				PrevIrql;
	t_BusEnumReportRep	rep = { 0 };
	//
	/////////////////////////////////////

ExAcquireFastMutex(&pFDO_DeviceData->m_Mutex);
	if (0 == pFDO_DeviceData->m_ulNumPDOs) {
		fstr("___ No devices to report! userIdx: %u\n", pReq->m_byIdx);
		ExReleaseFastMutex(&pFDO_DeviceData->m_Mutex);
		return (STATUS_NO_SUCH_DEVICE);
	}//~

	for (entry = pFDO_DeviceData->m_lstentListOfPDOs.Flink; entry != &pFDO_DeviceData->m_lstentListOfPDOs && !bDetect; entry = entry->Flink) {
		pdoData = CONTAINING_RECORD(entry, t_PDO_DeviceData, Link);
		if (pReq->m_byIdx == pdoData->m_ulPadIdx) {
			bDetect = pdoData->m_bIsPresent;
			break;
		}//~
	}//~rof
ExReleaseFastMutex(&pFDO_DeviceData->m_Mutex);

	if (!bDetect) {
#if 0
00000082	0.01520450	busenum.c(1483) protocol_bus_ReportDevice(): __# Device 0x00000002(2) INACTIVE	
00000083	0.01596970	[5360] CPseudoEventTransmitterActual.cpp(358) _send(): #_# SYSTEM DESIGN ERROR!!! Insufficient receive buffer size for received size. protoIdx: 0x73, required: 0x00000010, received: 0x00000000	
00000084	0.01604060	pnp.c(588) protocol_bus_PlugInDevice(): +++ Exposing PDO: ====== Serial: 2, Device: Root\PseudoControllerBusDevice, Length: 64	
#endif//~if0
		status = irpcopy(pIRP, &rep, sizeof (t_BusEnumReportRep), ulOutputLen);
		TEXEC(3, fstr("__# Device 0x%08X(%u) INACTIVE. status: 0x%08X\n", pReq->m_byIdx, pReq->m_byIdx, status));
		return (status);//return (STATUS_SUCCESS);	///return (STATUS_NO_SUCH_DEVICE);
	}//~

	if (pdoData->m_dwCallingPID != (DWORD)(DWORD_PTR)PsGetCurrentProcessId()) {
		TEXEC(3, fstr("__# Device 0x%08X(%u) INVALID PID\n", pReq->m_byIdx, pReq->m_byIdx));
		return (STATUS_SUCCESS);	///return (STATUS_ACCESS_DENIED);
	}//~

	// compare current report to last known report
	if (0 == memcmp(pdoData->m_byaReport, pReq->m_byData, sizeof (t_XinputGamePadClone))) {
		// same report data
		status = irpcopy(pIRP, &rep, sizeof (t_BusEnumReportRep), ulOutputLen);
		TEXEC(3, fstr("__# Device 0x%08X(%u) NO CHANGE. status: 0x%08X\n", pReq->m_byIdx, pReq->m_byIdx, status));
		return (status);
		///return (STATUS_SUCCESS);
	}//~
		
	/////////////////////////////////
KeAcquireSpinLock(&pdoData->m_lstentPendingQueueLock, &PrevIrql);
	{
		if (!IsListEmpty(&pdoData->m_lstentPendingQueue))
		{
			PLIST_ENTRY  le = RemoveHeadList(&pdoData->m_lstentPendingQueue);
			t_Pending_IRP* lr = CONTAINING_RECORD(le, t_Pending_IRP, Link);

			PendingIrp = lr->Irp;
			ExFreeToNPagedLookasideList(&g_LookAside, le);
		}
	}
KeReleaseSpinLock(&pdoData->m_lstentPendingQueueLock, PrevIrql);

	if (NULL == PendingIrp) {
		status = irpcopy(pIRP, &rep, sizeof (t_BusEnumReportRep), ulOutputLen);
		TEXEC(3, fstr("__# Device 0x%08X(%u) NO PENDING IRP. status: 0x%08X\n", pReq->m_byIdx, pReq->m_byIdx, status));
		return (status);//return (STATUS_SUCCESS);
	}//~

	//////////////////////////////////////////////
	// HW packet to kernel
	///!!!PrevIrql = KeRaiseIrqlToDpcLevel();
	KeRaiseIrql(DISPATCH_LEVEL, &PrevIrql);
	{
		IO_STACK_LOCATION* irpStack = IoGetCurrentIrpStackLocation(PendingIrp);
		URB* pHxp		= (PURB)irpStack->Parameters.Others.Argument1;
		UCHAR* Buffer	= (PUCHAR)pHxp->UrbBulkOrInterruptTransfer.TransferBuffer;
		pHxp->UrbBulkOrInterruptTransfer.TransferBufferLength = sizeof (t_XinputGamePadClone);

		// cp URB transfer buf, cache
		memcpy(Buffer, pReq->m_byData, sizeof (t_XinputGamePadClone));
		memcpy(pdoData->m_byaReport, pReq->m_byData, sizeof (t_XinputGamePadClone));

		PendingIrp->IoStatus.Status = STATUS_SUCCESS;
		IoCompleteRequest(PendingIrp, IO_NO_INCREMENT);
	}
	KeLowerIrql(PrevIrql);


	memcpy(&rep.m_feedback.wLeftMotorSpeed, &pdoData->m_byaRumble, sizeof (DWORD64));///sizeof (t_XIFeedback));

	///rep.m_feedback.wLeftMotorSpeed = HIWORD(pdoData->m_byaRumble);
	///rep.m_feedback.wRightMotorSpeed = LOWORD(pdoData->m_byaRumble);
	rep.m_feedback.wRingLight = pdoData->m_byRingLightNo;
	status = irpcopy(pIRP, &rep, sizeof (t_BusEnumReportRep), ulOutputLen);
	///TEXEC(1, odumpf(&rep, sizeof (t_BusEnumReportRep), "idx: %u", pReq->m_byIdx));
	///memcpy(pIRP->AssociatedIrp.SystemBuffer, &rep, sizeof (t_BusEnumReportRep));
	///pIRP->IoStatus.Information = sizeof (t_BusEnumReportRep);///////////// (sizeof (DWORD64) + sizeof (BYTE) + sizeof (BYTE));
	
	return (status);
}

NTSTATUS protocol_bus_IsDevicePluggedIn(t_BusEnumIsPlugedReq* pReq, t_FDO_DeviceData* pFDO_DeviceData, ULONG ulOutputLen, IRP* pIRP)//, PUCHAR pbyTransfer)
{
	BOOLEAN             bDetected = FALSE;
	LIST_ENTRY*			pEnt			= NULL;
	t_PDO_DeviceData*	pPDO_DeviceData = NULL;
	t_BusEnumIsPlugedRep	rep = {0};

ExAcquireFastMutex(&pFDO_DeviceData->m_Mutex);
	if (0 == pFDO_DeviceData->m_ulNumPDOs) {
		fstr("___ No devices to report!\n");
ExReleaseFastMutex(&pFDO_DeviceData->m_Mutex);
		return (STATUS_SUCCESS);
	}

	if (pReq->byIdx < 1 || 4 < pReq->byIdx) {
		fstr("___ Wrong Device Number! byIdx: %d\n", pReq->byIdx);
ExReleaseFastMutex(&pFDO_DeviceData->m_Mutex);
		return (STATUS_INVALID_DEVICE_REQUEST);
	}

	for (pEnt = pFDO_DeviceData->m_lstentListOfPDOs.Flink; pEnt != &pFDO_DeviceData->m_lstentListOfPDOs && !bDetected; pEnt = pEnt->Flink) {
		pPDO_DeviceData = CONTAINING_RECORD(pEnt, t_PDO_DeviceData, Link);
		if (pReq->byIdx == pPDO_DeviceData->m_ulPadIdx) {
			bDetected = TRUE;
			break;
		}
	}//~rof
ExReleaseFastMutex(&pFDO_DeviceData->m_Mutex);

	if (bDetected) {
		fstr("+++ Device %d is present\n", pReq->byIdx);
		rep.byState = 1;
	}
	else {
		fstr("#_# Device %d is NOT present\n", pReq->byIdx);
		rep.byState = 0;
	}
	return (irpcopy(pIRP, &rep, sizeof (t_BusEnumIsPlugedRep), ulOutputLen));
}

NTSTATUS protocol_bus_GetDeviceCreateProcID(t_BusEnumProcIDReq* pReq, t_FDO_DeviceData* pFDO_DeviceData, ULONG ulOutputLen, IRP* pIRP)///, PULONG pbyTransfer)
{
	///UNREFERENCED_PARAMETER(pbyTransfer);	// NO Using PARAM

	BOOLEAN             bDetected		= FALSE;
	LIST_ENTRY*			pEnt			= NULL;
	t_PDO_DeviceData*	pPDO_DeviceData	= NULL;
	t_BusEnumProcIDRep	rep = {0};
	rep.dwPID = 0x00;
	//
	/////////////////////////////////////

ExAcquireFastMutex(&pFDO_DeviceData->m_Mutex);
	if (pFDO_DeviceData->m_ulNumPDOs == 0) {
		fstr("___ No devices to report!\n");
ExReleaseFastMutex(&pFDO_DeviceData->m_Mutex);
		return (STATUS_SUCCESS);
	}

	if (pReq->byIdx < 1 || pReq->byIdx > 4) {
		fstr("___ Wrong Device Number! padIdx: %d\n", pReq->byIdx);
ExReleaseFastMutex(&pFDO_DeviceData->m_Mutex);
		return (STATUS_INVALID_DEVICE_REQUEST);
	}

	for (pEnt = pFDO_DeviceData->m_lstentListOfPDOs.Flink; pEnt != &pFDO_DeviceData->m_lstentListOfPDOs && !bDetected; pEnt = pEnt->Flink) {
		pPDO_DeviceData = CONTAINING_RECORD(pEnt, t_PDO_DeviceData, Link);
		if (pReq->byIdx == pPDO_DeviceData->m_ulPadIdx) {
			bDetected = TRUE;
			break;
		}
	}//~rof
ExReleaseFastMutex(&pFDO_DeviceData->m_Mutex);

	if (bDetected) {
		fstr("+++ Device %d is present (registered pid=%u)\n", pReq->byIdx, pPDO_DeviceData->m_dwCallingPID);
		rep.dwPID = pPDO_DeviceData->m_dwCallingPID;
	}
	else {
		fstr("#_# Device %d is NOT present\n", pReq->byIdx);
	}

	return (irpcopy(pIRP, &rep, sizeof (t_BusEnumProcIDRep), ulOutputLen));
}

NTSTATUS protocol_bus_GetNumberOfEmptySlots(t_FDO_DeviceData* pFDO_DeviceData, ULONG ulOutputLen, IRP* pIRP)//, PUCHAR pbyTransfer)
{
	UCHAR				ubyCount = 4;	// sa: XInput.h XUSER_MAX_COUNT(4). wireless cntlr: 8
	LIST_ENTRY*			pEnt	= NULL;
	t_PDO_DeviceData*	pPDO_DeviceData = NULL;

ExAcquireFastMutex(&pFDO_DeviceData->m_Mutex);
	if (0 < pFDO_DeviceData->m_ulNumPDOs) {
		for (pEnt = pFDO_DeviceData->m_lstentListOfPDOs.Flink; pEnt != &pFDO_DeviceData->m_lstentListOfPDOs; pEnt = pEnt->Flink) {
			pPDO_DeviceData = CONTAINING_RECORD(pEnt, t_PDO_DeviceData, Link);
			if (0 != pPDO_DeviceData->m_ulPadIdx) {
				ubyCount--;
			}
		}//~rof
	}//~fi
ExReleaseFastMutex(&pFDO_DeviceData->m_Mutex);

	t_BusEnumFindEmptySlotRes res;
	res.byEmptySlot = ubyCount;
	return (irpcopy(pIRP, &res, sizeof (t_BusEnumFindEmptySlotRes), ulOutputLen));
}


