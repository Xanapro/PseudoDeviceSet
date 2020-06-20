#include "./busenum.h"
#include "./ntmiscdbg.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, Bus_PDO_PnP)
#pragma alloc_text(PAGE, bus_PDO_QueryDeviceCaps)
#pragma alloc_text(PAGE, bus_PDO_QueryDeviceId)
#pragma alloc_text(PAGE, bus_PDO_QueryDeviceText)
#pragma alloc_text(PAGE, bus_PDO_QueryDeviceRelations)
#pragma alloc_text(PAGE, bus_PDO_QueryBusInformation)
#pragma alloc_text(PAGE, bus_GetDeviceCapabilities)
#pragma alloc_text(PAGE, bus_PDO_QueryInterface)
#endif//~defined(ALLOC_PRAGMA)



NTSTATUS Bus_PDO_PnP(__in DEVICE_OBJECT* pDeviceObject, __in IRP* pIRP, __in IO_STACK_LOCATION* pIRP_Stack, __in t_PDO_DeviceData* pPDO_DeviceData)
{
	PAGED_CODE();
	//
	////////////////////////////////
	// see also: /opt/Windows-driver-samples/serial/serenum/pnp.c

	fstr("+++ CALLING\n");
	
	NTSTATUS	status	= STATUS_SUCCESS;
	switch (pIRP_Stack->MinorFunction) {
	case IRP_MN_START_DEVICE:	// see also: https://blogs.msdn.microsoft.com/jpwdkblog/2009/07/06/31532/ 
	{
		pPDO_DeviceData->m_eDevicePowerState = PowerDeviceD0;
		pPDO_DeviceData->m_ePreviousPnPState = pPDO_DeviceData->m_eDevicePnPState;	// ! DONT REPLACE !
		pPDO_DeviceData->m_eDevicePnPState	= E_DPNP_STARTED;						// ! DONT REPLACE !

		status = IoRegisterDeviceInterface(pDeviceObject, (LPGUID)&GUID_DEVINTERFACE_USB_DEVICE, NULL, &pPDO_DeviceData->m_ucInterfaceName);
		if (NT_SUCCESS(status)) {
			// Device is starting
			IoSetDeviceInterfaceState(&pPDO_DeviceData->m_ucInterfaceName, TRUE);
			pPDO_DeviceData->m_bIsStarted = TRUE;
		}//~fi
		else {
			fstr("#_# IoRegisterDeviceInterface() status: 0x%08X(%d)\n", status, status);
		}//~esle
		status = STATUS_SUCCESS;	// REWRITE NTSTATUS
	}
	break;

	case IRP_MN_STOP_DEVICE:
	{
		SET_NEW_PNP_STATE(pPDO_DeviceData, E_DPNP_STOPPED);
		//pPDO_DeviceData->m_ePreviousPnPState = pPDO_DeviceData->m_eDevicePnPState;
		//pPDO_DeviceData->m_eDevicePnPState = E_DPNP_STOPPED;
		pPDO_DeviceData->m_bIsStarted = FALSE;
	}
	break;

	case IRP_MN_QUERY_STOP_DEVICE:
	{
		SET_NEW_PNP_STATE(pPDO_DeviceData, E_DPNP_STOP_PENDING);
		//pPDO_DeviceData->m_ePreviousPnPState = pPDO_DeviceData->m_eDevicePnPState;
		//pPDO_DeviceData->m_eDevicePnPState = E_DPNP_STOP_PENDING;
	}
	break;

	case IRP_MN_CANCEL_STOP_DEVICE:
	{
		if (E_DPNP_STOP_PENDING == pPDO_DeviceData->m_eDevicePnPState) {
			RESTORE_PREVIOUS_PNP_STATE(pPDO_DeviceData);
			//pPDO_DeviceData->m_eDevicePnPState = pPDO_DeviceData->m_ePreviousPnPState;
		}
	}
	break;

	case IRP_MN_QUERY_REMOVE_DEVICE:
	{
		///SET_NEW_PNP_STATE(pPDO_DeviceData, E_DPNP_REMOVE_PENDING);	// Data remains depending on the timing
		SET_NEW_PNP_STATE(pPDO_DeviceData, E_DPNP_STOP_PENDING);	//!!!
		//pPDO_DeviceData->m_ePreviousPnPState = pPDO_DeviceData->m_eDevicePnPState;
		//pPDO_DeviceData->m_eDevicePnPState = E_DPNP_STOP_PENDING;
	}
	break;

	case IRP_MN_CANCEL_REMOVE_DEVICE:
	{
		if (E_DPNP_REMOVE_PENDING == pPDO_DeviceData->m_eDevicePnPState) {
			RESTORE_PREVIOUS_PNP_STATE(pPDO_DeviceData);
			//pPDO_DeviceData->m_eDevicePnPState = pPDO_DeviceData->m_ePreviousPnPState;
		}
	}
	break;

	case IRP_MN_SURPRISE_REMOVAL:
	{
		///
		SET_NEW_PNP_STATE(pPDO_DeviceData, E_DPNP_SURPRISE_REMOVE_PENDING);
		//SET_NEW_PNP_STATE(pPDO_DeviceData, E_DPNP_STOP_PENDING);
		//pPDO_DeviceData->m_ePreviousPnPState = pPDO_DeviceData->m_eDevicePnPState;
		//pPDO_DeviceData->m_eDevicePnPState = E_DPNP_STOP_PENDING;
		IoSetDeviceInterfaceState(&pPDO_DeviceData->m_ucInterfaceName, FALSE);

		if (!pPDO_DeviceData->m_bIsReportedMissing) {
			break;
		}
		
		SET_NEW_PNP_STATE(pPDO_DeviceData, E_DPNP_DELETED);
		//pPDO_DeviceData->m_ePreviousPnPState = pPDO_DeviceData->m_eDevicePnPState;
		//pPDO_DeviceData->m_eDevicePnPState = E_DPNP_DELETED;
		
		if (NULL != pPDO_DeviceData->m_pParentFDO) {
			t_FDO_DeviceData*	pFDO_Data = pPDO_DeviceData->m_pParentFDO->DeviceExtension;
ExAcquireFastMutex(&pFDO_Data->m_Mutex);
			RemoveEntryList(&pPDO_DeviceData->Link);
ExReleaseFastMutex(&pFDO_Data->m_Mutex);
		}//~fi
		
		bus_DestroyPDO(pPDO_DeviceData->m_pSelfDeviceObj->DeviceExtension, pPDO_DeviceData);
		fstr("\tDeleting PDO: 0x%p\n", pPDO_DeviceData->m_pSelfDeviceObj);
		IoDeleteDevice(pPDO_DeviceData->m_pSelfDeviceObj);

	}//~case IRP_MN_SURPRISE_REMOVAL:
	break;

	case IRP_MN_REMOVE_DEVICE:
	{
		IoSetDeviceInterfaceState(&pPDO_DeviceData->m_ucInterfaceName, FALSE);

		if (pPDO_DeviceData->m_bIsReportedMissing) {
			SET_NEW_PNP_STATE(pPDO_DeviceData, E_DPNP_DELETED);
			//pPDO_DeviceData->m_ePreviousPnPState = pPDO_DeviceData->m_eDevicePnPState;
			//pPDO_DeviceData->m_eDevicePnPState = E_DPNP_DELETED;
			if (pPDO_DeviceData->m_pParentFDO) {
				t_FDO_DeviceData*	pFDO_Data = (t_FDO_DeviceData*)pDeviceObject->DeviceExtension;
ExAcquireFastMutex(&pFDO_Data->m_Mutex);
				RemoveEntryList(&pPDO_DeviceData->Link);
ExReleaseFastMutex(&pFDO_Data->m_Mutex);

				///	status = bus_DestroyPDO(pDeviceObject, pPDO_DeviceData);
				bus_DestroyPDO(pPDO_DeviceData->m_pSelfDeviceObj->DeviceExtension, pPDO_DeviceData);
				fstr("\tDeleting PDO: 0x%p\n", pPDO_DeviceData->m_pSelfDeviceObj);
				IoDeleteDevice(pPDO_DeviceData->m_pSelfDeviceObj);
			}//~fi
			break;
		}//~fi

		if (pPDO_DeviceData->m_bIsPresent) {
			SET_NEW_PNP_STATE(pPDO_DeviceData, E_DPNP_STARTED);
			//pPDO_DeviceData->m_ePreviousPnPState = pPDO_DeviceData->m_eDevicePnPState;
			//pPDO_DeviceData->m_eDevicePnPState = E_DPNP_STARTED;
		}
		else {
			fstr("___ !pPDO_DeviceData->m_bIsPresent !!! SYSTEM DESIGN ERROR? !!!\n");
		}//~fi
	}//~case IRP_MN_REMOVE_DEVICE:
	break;

	case IRP_MN_QUERY_CAPABILITIES:
		status = bus_PDO_QueryDeviceCaps(pPDO_DeviceData, pIRP);
		break;

	case IRP_MN_QUERY_ID:
		fstr("\tQueryId Type: %s\n", DbgDeviceIDString(pIRP_Stack->Parameters.QueryId.IdType));
		status = bus_PDO_QueryDeviceId(pPDO_DeviceData, pIRP);
		break;

	case IRP_MN_QUERY_DEVICE_RELATIONS:
	{
		fstr("\tBuspdo.c: QueryDeviceRelation Type: %s\n", DbgDeviceRelationString(pIRP_Stack->Parameters.QueryDeviceRelations.Type));
		status = bus_PDO_QueryDeviceRelations(pPDO_DeviceData, pIRP);
	}//~case IRP_MN_QUERY_DEVICE_RELATIONS:
	break;

	case IRP_MN_QUERY_DEVICE_TEXT:
	{
		status = bus_PDO_QueryDeviceText(pPDO_DeviceData, pIRP);
	}//~case IRP_MN_QUERY_DEVICE_TEXT:
	break;

	case IRP_MN_QUERY_RESOURCES:
		///status = bus_PDO_QueryResources(pPDO_DeviceData, pIRP);
		status = pIRP->IoStatus.Status;
		break;

	case IRP_MN_QUERY_RESOURCE_REQUIREMENTS:
		///status = bus_PDO_QueryResourceRequirements(pPDO_DeviceData, pIRP);
		status = STATUS_SUCCESS;
		break;

	case IRP_MN_QUERY_BUS_INFORMATION:
		status = bus_PDO_QueryBusInformation(pPDO_DeviceData, pIRP);
		break;

	case IRP_MN_DEVICE_USAGE_NOTIFICATION:
		status = STATUS_UNSUCCESSFUL;
		break;

	case IRP_MN_EJECT:
		pPDO_DeviceData->m_bIsPresent = FALSE;
		status = STATUS_SUCCESS;
		break;

	case IRP_MN_QUERY_INTERFACE:
		status = bus_PDO_QueryInterface(pPDO_DeviceData, pIRP);
		break;

	case IRP_MJ_FILE_SYSTEM_CONTROL:
	case IRP_MJ_DEVICE_CONTROL:
	case IRP_MJ_INTERNAL_DEVICE_CONTROL:
	case IRP_MJ_SHUTDOWN:
	case IRP_MJ_CLEANUP:
	case IRP_MJ_QUERY_SECURITY:
	case IRP_MJ_DEVICE_CHANGE:
	case IRP_MJ_QUERY_QUOTA:
	case IRP_MJ_SET_QUOTA:
	case IRP_MJ_PNP:	///IRP_MJ_PNP_POWERP_MJ_PNP//Obsolete....

	default:
		status = pIRP->IoStatus.Status;
		break;
	}//~hctiws

	pIRP->IoStatus.Status = status;
	IoCompleteRequest(pIRP, IO_NO_INCREMENT);

	return (status);
}

NTSTATUS bus_PDO_QueryDeviceCaps(__in t_PDO_DeviceData* pPDO_DeviceData, __in IRP* pIRP)
{
	PAGED_CODE();
	//
	///////////////////////////////////////

	IO_STACK_LOCATION*	pIRPStack	= IoGetCurrentIrpStackLocation(pIRP);
	fstr("+++ CALLED\n");
	if (NULL == pIRPStack) {
		fstr("\tNULL == pIRPStack !!! NO REQUEST I/O PACKET !!!\n");
	}//~fi

	// sa: https://docs.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/ns-wdm-_device_capabilities
	DEVICE_CAPABILITIES* pDeviceCapabilities	= pIRPStack->Parameters.DeviceCapabilities.Capabilities;
	if (NULL == pDeviceCapabilities) {
		fstr("___ NULL == pDeviceCapabilities !!! SYSTEM DESIGN ERROR !!!\n");
		return (STATUS_FWP_NULL_POINTER);
	}//~fi
	if (1 != pDeviceCapabilities->Version || sizeof (DEVICE_CAPABILITIES) > pDeviceCapabilities->Size) {
		return (STATUS_UNSUCCESSFUL);
	}//~fi

	t_FDO_DeviceData* pFDO_Data = pPDO_DeviceData->m_pParentFDO->DeviceExtension;
	if (NULL == pFDO_Data) {
		fstr("___ NULL == pFDO_Data !!! SYSTEM DESIGN ERROR !!!\n");
		return (STATUS_FWP_NULL_POINTER);
	}//~fi

	DEVICE_CAPABILITIES	parentCapabilities;
	NTSTATUS status = bus_GetDeviceCapabilities(pFDO_Data->m_pNextLowerDriver, &parentCapabilities);
	if (!NT_SUCCESS(status)) {
		fstr("\tQueryDeviceCaps failed\n");
		return (status);
	}//~fi

	RtlCopyMemory(pDeviceCapabilities->DeviceState, parentCapabilities.DeviceState, (PowerSystemShutdown + 1) * sizeof (DEVICE_POWER_STATE));
	
	    pDeviceCapabilities->DeviceState[PowerSystemWorking]	= PowerDeviceD0;
	if (pDeviceCapabilities->DeviceState[PowerSystemSleeping1] != PowerDeviceD0) {
		pDeviceCapabilities->DeviceState[PowerSystemSleeping1]  = PowerDeviceD1;
	}//~fi
	if (pDeviceCapabilities->DeviceState[PowerSystemSleeping2] != PowerDeviceD0) {
		pDeviceCapabilities->DeviceState[PowerSystemSleeping2]  = PowerDeviceD3;
	}//~fi
	if (pDeviceCapabilities->DeviceState[PowerSystemSleeping3] != PowerDeviceD0) {
		pDeviceCapabilities->DeviceState[PowerSystemSleeping3]  = PowerDeviceD3;
	}//~fi
	    pDeviceCapabilities->DeviceWake = PowerDeviceD1;

	// sa: https://docs.microsoft.com/en-us/windows-hardware/drivers/kernel/device-sleeping-states
	// DeviceD1 - DecodeIoOnBoot bit field
	pDeviceCapabilities->DeviceD1	= 0x01;		/// !!
	pDeviceCapabilities->DeviceD2	= 0x00;

	pDeviceCapabilities->WakeFromD0 = 0x00;
	pDeviceCapabilities->WakeFromD1 = 0x01;		/// !!
	pDeviceCapabilities->WakeFromD2 = 0x00;
	pDeviceCapabilities->WakeFromD3	= 0x00;

	pDeviceCapabilities->D1Latency	= 0x00;
	pDeviceCapabilities->D2Latency	= 0x00;
	pDeviceCapabilities->D3Latency	= 0x00;

	pDeviceCapabilities->EjectSupported		= 0x00;
	pDeviceCapabilities->HardwareDisabled	= 0x00;
	pDeviceCapabilities->Removable			= 0x01;
	pDeviceCapabilities->SurpriseRemovalOK	= 0x01;	// [Safely Remove Hardware and Eject Media] noeffect?
	pDeviceCapabilities->UniqueID			= 0x01;
	pDeviceCapabilities->SilentInstall		= 0x00;
	pDeviceCapabilities->Address			= pPDO_DeviceData->m_ulPadIdx;

	return (status);//STATUS_SUCCESS;
}

NTSTATUS bus_PDO_QueryDeviceId(__in t_PDO_DeviceData* pPDO_DeviceData, __in IRP* pIRP)
{
	NTSTATUS	status	= STATUS_SUCCESS;
	///
	PWCHAR		pwcBuf	= NULL;
	ULONG			ulLen	= 0;
	PAGED_CODE();
	//
	/////////////////////////////////////////

	IO_STACK_LOCATION* pIRPStack = IoGetCurrentIrpStackLocation(pIRP);
	if (NULL == pIRPStack) {
		fstr("\tNULL == pIRPStack !!! NOTHING REQUEST I/O PACKET !!!\n");
		// sa: http://accelart.jp/blog/NTSTATUSErrMsgJa.html
		return (STATUS_MORE_PROCESSING_REQUIRED);
	}//~fi

	fstr("!!! pIRPStack->Parameters.QueryId.IdType: %d\n", pIRPStack->Parameters.QueryId.IdType);


	// sa: C:\opt\Windows-driver-samples\serial\serenum\pnp.c
	switch (pIRPStack->Parameters.QueryId.IdType) {
	case BusQueryDeviceID:	// see also: wdm.h
	case BusQueryHardwareIDs:
	case BusQueryCompatibleIDs:
	{
		//!!! DEVICE_ID | HARDWARE_ID | COMPATIBLE_ID !!!
		UNICODE_STRING*	punicID	= NULL;
		switch (pIRPStack->Parameters.QueryId.IdType) {
		case BusQueryDeviceID:		punicID	= pPDO_DeviceData->m_puniHardwareID;	break;
		case BusQueryHardwareIDs:	punicID	= pPDO_DeviceData->m_puniHardwareID;	break;
		case BusQueryCompatibleIDs:	punicID	= pPDO_DeviceData->m_puniHardwareID;	break;
		}//~hctiws
		
		/// FORCE REWRITE
		///punicID	= pPDO_DeviceData->m_puniHardwareID;
		if (NULL == punicID) {
			fstr("___ IdType: %d, NULL == punicID !!!\n", pIRPStack->Parameters.QueryId.IdType);
			punicID	= pPDO_DeviceData->m_puniHardwareID;
			///return (STATUS_FWP_NULL_POINTER);
		}//~fi

		//ulLen = sizeof (K_BUSENUM_COMPATIBLE_IDS);
		ulLen	= punicID->Length;
		pwcBuf	= ExAllocatePoolWithTag(PagedPool, ulLen, K_BUSENUM_POOL_TAG);
		if (NULL == pwcBuf) {
			status = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}
		RtlCopyMemory(pwcBuf, K_BUSENUM_COMPATIBLE_IDS, ulLen);
		pIRP->IoStatus.Information = (ULONG_PTR)pwcBuf;
	}//~case BusQuery[ DeviceID / HardwareIDs / CompatibleIDs ]
	break;

	case BusQueryInstanceID:
	{
		// actually do L"%07d", because  8 * sizeof (wchar_t) // (contain L'\0')
		ulLen = 8 * sizeof (wchar_t);
		pwcBuf = ExAllocatePoolWithTag(PagedPool, ulLen, K_BUSENUM_POOL_TAG);
		if (NULL == pwcBuf) {
			status = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}//~fi

		RtlStringCchPrintfW(pwcBuf, ulLen / sizeof (WCHAR), L"%07d", pPDO_DeviceData->m_ulPadIdx);
		pIRP->IoStatus.Information = (ULONG_PTR)pwcBuf;
		fstr("\tInstanceID: %ws, PDO_DeviceData->m_ulPadIdx: %07d(0x%08X)\n", pwcBuf, pPDO_DeviceData->m_ulPadIdx, pPDO_DeviceData->m_ulPadIdx);
	}
	break;

	case BusQueryDeviceSerialNumber:
		fstr("#+# [BusQueryDeviceSerialNumbe]\n");
		status = pIRP->IoStatus.Status;
		break;
	case BusQueryContainerID:
		fstr("#+# [BusQueryContainerID]\n");
		status = pIRP->IoStatus.Status;
		break;
	default:
		status = pIRP->IoStatus.Status;
		break;
	}//~hctiws

	return (status);
}

NTSTATUS bus_PDO_QueryDeviceText(__in t_PDO_DeviceData* pPDO_DeviceData, __in IRP* pIRP)
{
	UNREFERENCED_PARAMETER(pPDO_DeviceData);
	NTSTATUS	status	= STATUS_SUCCESS;
	PWCHAR		pwBuf	= NULL;
	USHORT		usLen	= 0;
	IO_STACK_LOCATION*	pStack	= NULL;
	PAGED_CODE();
	//
	/////////////////////////////////////////

	pStack = IoGetCurrentIrpStackLocation(pIRP);
	switch (pStack->Parameters.QueryDeviceText.DeviceTextType) {
	case DeviceTextDescription:
	{
		// already allocated?
		if (pIRP->IoStatus.Information) {
			status = STATUS_SUCCESS;
			break;
		}

		//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		usLen = (USHORT)(wcslen(K_PRODUCT) + 1) * sizeof (WCHAR);	// +1: NULL CHAR
		pwBuf = ExAllocatePoolWithTag(PagedPool, usLen, K_BUSENUM_POOL_TAG);
		if (NULL == pwBuf) {
			status = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

		// K_PRODUCT ?
		RtlStringCchPrintfW(pwBuf, usLen / sizeof (WCHAR), L"%ws", K_PRODUCT);
		fstr("\tDeviceTextDescription: %ws\n", pwBuf);
		pIRP->IoStatus.Information = (ULONG_PTR)pwBuf;
		status = STATUS_SUCCESS;
	}
	break;
	default:
		status = pIRP->IoStatus.Status;
		break;
	}//~hctiws

	return status;
}

NTSTATUS bus_PDO_QueryDeviceRelations(__in t_PDO_DeviceData* pPDO_DeviceData, __in IRP* pIRP)
{
	NTSTATUS	status	= STATUS_SUCCESS;
	DEVICE_RELATIONS*	pDeviceRelations	= NULL;
	IO_STACK_LOCATION*	pStack	= NULL;
	PAGED_CODE();
	//
	/////////////////////////////////////

	pStack = IoGetCurrentIrpStackLocation(pIRP);
	switch (pStack->Parameters.QueryDeviceRelations.Type) {
	case TargetDeviceRelation:
	{
		pDeviceRelations = (PDEVICE_RELATIONS)pIRP->IoStatus.Information;
		if (NULL != pDeviceRelations) {
			// ALREADY PROCESSED
		}//~

		pDeviceRelations = (PDEVICE_RELATIONS)ExAllocatePoolWithTag(PagedPool, sizeof (DEVICE_RELATIONS), K_BUSENUM_POOL_TAG);
		if (NULL == pDeviceRelations) {
			status = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

		pDeviceRelations->Count = 1;
		pDeviceRelations->Objects[0] = pPDO_DeviceData->m_pSelfDeviceObj;

		ObReferenceObject(pPDO_DeviceData->m_pSelfDeviceObj);

		fstr("+++ case TargetDeviceRelation: pPDO_DeviceData->m_ulPadIdx: %d, PDO->Present: %d\n", pPDO_DeviceData->m_ulPadIdx, pPDO_DeviceData->m_bIsPresent);

		status = STATUS_SUCCESS;
		pIRP->IoStatus.Information = (ULONG_PTR)pDeviceRelations;

	}//~case TargetDeviceRelation:
	break;

	////////////////////
	// PDO option
	case BusRelations:
	case EjectionRelations:
	case RemovalRelations:
	default:
		status = pIRP->IoStatus.Status;
		break;
	}//~htctiws

	return status;
}

NTSTATUS bus_PDO_QueryBusInformation(__in t_PDO_DeviceData* pPDO_DeviceData, __in IRP* pIRP)
{
	UNREFERENCED_PARAMETER(pPDO_DeviceData);
	PNP_BUS_INFORMATION*	pBusInfo = NULL;
	PAGED_CODE();
	//
	/////////////////////////////////

	pBusInfo = ExAllocatePoolWithTag(PagedPool, sizeof (PNP_BUS_INFORMATION), K_BUSENUM_POOL_TAG);
	if (pBusInfo == NULL) {
		return (STATUS_INSUFFICIENT_RESOURCES);
	}

	pBusInfo->BusTypeGuid   = GUID_BUS_TYPE_USB;
	pBusInfo->LegacyBusType = PNPBus;		// sa: wdm.h enum INTERFACE_TYPE
	pBusInfo->BusNumber     = 0;

	pIRP->IoStatus.Information = (ULONG_PTR)pBusInfo;
	return (STATUS_SUCCESS);
}

// sa: https://docs.microsoft.com/en-us/windows-hardware/drivers/ifs/attaching-the-filter-device-object-to-the-target-device-object
NTSTATUS bus_GetDeviceCapabilities(__in DEVICE_OBJECT* pDeviceObject, __out DEVICE_CAPABILITIES* pDeviceCapabilities)
{
	NTSTATUS			status	= STATUS_SUCCESS;
	KEVENT				evtPnP;
	IO_STATUS_BLOCK		ioStatusBlock;
	IRP*				pIRP_PnP	= NULL;
	IO_STACK_LOCATION*	pIRP_Stack	= NULL;
	DEVICE_OBJECT*		pTgtDeviceObject	= NULL;
	PAGED_CODE();
	//
	//////////////////////////////////

	//@{
	memset(pDeviceCapabilities, 0, sizeof (DEVICE_CAPABILITIES));
	pDeviceCapabilities->Size     = sizeof (DEVICE_CAPABILITIES);
	pDeviceCapabilities->Version  = 1;
	pDeviceCapabilities->Address  = ULONG_MAX;
	pDeviceCapabilities->UINumber = ULONG_MAX;
	//@}

	KeInitializeEvent(&evtPnP, NotificationEvent, FALSE);

	pTgtDeviceObject = IoGetAttachedDeviceReference(pDeviceObject);
	pIRP_PnP = IoBuildSynchronousFsdRequest(IRP_MJ_PNP, pTgtDeviceObject, NULL, 0, NULL, &evtPnP, &ioStatusBlock);
	if (pIRP_PnP == NULL) {
		status = STATUS_INSUFFICIENT_RESOURCES;
		ObDereferenceObject(pTgtDeviceObject);
		return (status);
	}

	pIRP_PnP->IoStatus.Status = STATUS_NOT_SUPPORTED;
	/////////
	//
	pIRP_Stack = IoGetNextIrpStackLocation(pIRP_PnP);
	memset(pIRP_Stack, 0, sizeof (IO_STACK_LOCATION));

	pIRP_Stack->MajorFunction = IRP_MJ_PNP;
	pIRP_Stack->MinorFunction = IRP_MN_QUERY_CAPABILITIES;
	//@{
	pIRP_Stack->Parameters.DeviceCapabilities.Capabilities = pDeviceCapabilities;
	//@}

	status = IoCallDriver(pTgtDeviceObject, pIRP_PnP);
	if (STATUS_PENDING == status) {
		KeWaitForSingleObject(&evtPnP, Executive, KernelMode, FALSE, NULL);		// no timedout.
		status = ioStatusBlock.Status;
	}

	ObDereferenceObject(pTgtDeviceObject);
	return (status);
}


VOID CB_InterfaceReference(__in PVOID pvContext)
{
	InterlockedIncrement((LONG*) & ((t_PDO_DeviceData*)pvContext)->m_ulInterfaceRefCount);
}

VOID CB_InterfaceDereference(__in PVOID pvContext)
{
	InterlockedDecrement((LONG*) & ((t_PDO_DeviceData*)pvContext)->m_ulInterfaceRefCount);
}


NTSTATUS USB_BUSIFFN CB_QueryBusInformation(__in PVOID pvBusContext, __in ULONG ulLevel, __inout PVOID pvBusInformationBuffer, __inout PULONG pulBusInformationBufferLength, __out PULONG pulBusInformationActualLength)
{
	UNREFERENCED_PARAMETER(pvBusContext);
	UNREFERENCED_PARAMETER(ulLevel);
	UNREFERENCED_PARAMETER(pvBusInformationBuffer);
	UNREFERENCED_PARAMETER(pulBusInformationBufferLength);
	UNREFERENCED_PARAMETER(pulBusInformationActualLength);
	//
	/////////////////////////////////////

	fstr("+++ CB_QueryBusInformation: STATUS_UNSUCCESSFUL\n");
	return (STATUS_UNSUCCESSFUL);
}

NTSTATUS USB_BUSIFFN CB_SubmitIsoOutUrb(__in PVOID pvBusContext, __in URB* pURB)
{
	UNREFERENCED_PARAMETER(pvBusContext);
	UNREFERENCED_PARAMETER(pURB);
	//
	/////////////////////////////////////

	fstr("+++ CB_SubmitIsoOutUrb: STATUS_UNSUCCESSFUL\n");
	return (STATUS_UNSUCCESSFUL);
}

NTSTATUS USB_BUSIFFN CB_QueryBusTime(__in PVOID pvBusContext, __inout PULONG pulCurrentUsbFrame)
{
	UNREFERENCED_PARAMETER(pvBusContext);
	UNREFERENCED_PARAMETER(pulCurrentUsbFrame);
	//
	/////////////////////////////////////

	fstr("+++ CB_QueryBusTime: STATUS_UNSUCCESSFUL\n");
	return (STATUS_UNSUCCESSFUL);
}

VOID USB_BUSIFFN CB_GetUSBDIVersion(__in PVOID pvBusContext, __inout USBD_VERSION_INFORMATION* pUSBD_VerInfo, __inout PULONG pulHcdCapabilities)
{
	UNREFERENCED_PARAMETER(pvBusContext);
	//
	/////////////////////////////////////

#define K_USBDI_VERSION		(0x0500)
#define K_USB_VERSION		(0x0200)
	fstr("+++ GetUSBDIVersion: 0x%04X, 0x%04X\n", K_USBDI_VERSION, K_USB_VERSION);

	if (NULL != pUSBD_VerInfo) {
		pUSBD_VerInfo->USBDI_Version			= K_USBDI_VERSION;	// Usbport
		pUSBD_VerInfo->Supported_USB_Version	= K_USB_VERSION;	// USB 2.0
	}
	if (NULL != pulHcdCapabilities) {
		*pulHcdCapabilities = 0;
	}
}

// USB_DI_V1
BOOLEAN USB_BUSIFFN CB_IsDeviceHighSpeed(__in PVOID pvBusContext)
{
	UNREFERENCED_PARAMETER(pvBusContext);
	//
	/////////////////////////////////////

	fstr("+++ CB_IsDeviceHighSpeed: TRUE\n");
	return (TRUE);
}

// USB_DI_V2
NTSTATUS USB_BUSIFFN CB_EnumLogEntry(__in PVOID pvBusContext, __in ULONG puDriverTag, __in ULONG ulEnumTag, __in ULONG ulP1, __in ULONG ulP2)
{
	UNREFERENCED_PARAMETER(pvBusContext);
	UNREFERENCED_PARAMETER(puDriverTag);
	UNREFERENCED_PARAMETER(ulEnumTag);
	UNREFERENCED_PARAMETER(ulP1);
	UNREFERENCED_PARAMETER(ulP2);
	return (STATUS_UNSUCCESSFUL);
}



// USB_DI_V3
NTSTATUS USB_BUSIFFN CB_QueryBusTimeEx(__in PVOID pvBusContext, __out PULONG puHighSpeedFrameCounter)
{
	UNREFERENCED_PARAMETER(pvBusContext);
	UNREFERENCED_PARAMETER(puHighSpeedFrameCounter);
	return (STATUS_UNSUCCESSFUL);
}

// USB_DI_V3
NTSTATUS USB_BUSIFFN CB_QueryControllerType(_In_opt_ PVOID pvBusContext, _Out_opt_ PULONG puHcdiOptionFlags, _Out_opt_ PUSHORT pusPciVendorId, _Out_opt_ PUSHORT pusPciDeviceId, _Out_opt_ PUCHAR puPciClass, _Out_opt_ PUCHAR puPciSubClass, _Out_opt_ PUCHAR puPciRevisionId, _Out_opt_ PUCHAR puPciProgIf)
{
	UNREFERENCED_PARAMETER(pvBusContext);
	UNREFERENCED_PARAMETER(puHcdiOptionFlags);
	UNREFERENCED_PARAMETER(pusPciVendorId);
	UNREFERENCED_PARAMETER(pusPciDeviceId);
	UNREFERENCED_PARAMETER(puPciClass);
	UNREFERENCED_PARAMETER(puPciSubClass);
	UNREFERENCED_PARAMETER(puPciRevisionId);
	UNREFERENCED_PARAMETER(puPciProgIf);
	return (STATUS_UNSUCCESSFUL);
}


NTSTATUS bus_PDO_QueryInterface(__in t_PDO_DeviceData* pPDO_DeviceData, __in IRP* pIRP)
{
	NTSTATUS	status	= STATUS_SUCCESS;
	GUID*		pGUID	= NULL;
	IO_STACK_LOCATION*	pIRP_Stack	= NULL;
	USB_BUS_INTERFACE_USBDI_V0* pInterfaceV0 = NULL;
	USB_BUS_INTERFACE_USBDI_V1* pInterfaceV1 = NULL;
	USB_BUS_INTERFACE_USBDI_V2* pInterfaceV2 = NULL;
	USB_BUS_INTERFACE_USBDI_V3* pInterfaceV3 = NULL;
	PAGED_CODE();
	//
	///////////////////////////////////////////

	pIRP_Stack      = IoGetCurrentIrpStackLocation(pIRP);
	pGUID = (GUID*)pIRP_Stack->Parameters.QueryInterface.InterfaceType;
	if (!IsEqualGUID(pGUID, (PVOID)&USB_BUS_INTERFACE_USBDI_GUID)) {
		///---fstr("___ Invalid GUID: " GUID_FORMAT ", ver: %d\n", GUID_ARGP(pGUID), pIRP_Stack->Parameters.QueryInterface.Version);
		status = pIRP->IoStatus.Status;
		return (status);
	}//~fi
		
	fstr("+++ USB_BUS_INTERFACE_USBDI_GUID: USB_BUSIF_USBDI_VERSION_%u, Size:%u(v0:%u, v1:%u, v2:%u, v3:%u)\n"
		, (unsigned)pIRP_Stack->Parameters.QueryInterface.Version
		, pIRP_Stack->Parameters.QueryInterface.Size
		, sizeof (USB_BUS_INTERFACE_USBDI_V0)
		, sizeof (USB_BUS_INTERFACE_USBDI_V1)
		, sizeof (USB_BUS_INTERFACE_USBDI_V2)
		, sizeof (USB_BUS_INTERFACE_USBDI_V3)
	);
	fstr("+++ GUID: " GUID_FORMAT "\n", GUID_ARGP(pGUID));

#define CHK_USB_STRUCT_SIZE_BY_VER(ver, structName) (pIRP_Stack->Parameters.QueryInterface.Version == ver && pIRP_Stack->Parameters.QueryInterface.Size < sizeof (structName))
	if (   CHK_USB_STRUCT_SIZE_BY_VER(USB_BUSIF_USBDI_VERSION_0, USB_BUS_INTERFACE_USBDI_V0)
		|| CHK_USB_STRUCT_SIZE_BY_VER(USB_BUSIF_USBDI_VERSION_1, USB_BUS_INTERFACE_USBDI_V1)
		|| CHK_USB_STRUCT_SIZE_BY_VER(USB_BUSIF_USBDI_VERSION_2, USB_BUS_INTERFACE_USBDI_V2)
		|| CHK_USB_STRUCT_SIZE_BY_VER(USB_BUSIF_USBDI_VERSION_3, USB_BUS_INTERFACE_USBDI_V3)
		) {
		return (STATUS_INVALID_PARAMETER);
	}//~fi
#undef CHK_USB_STRUCT_SIZE_BY_VER	//~

	switch (pIRP_Stack->Parameters.QueryInterface.Version) {
	case USB_BUSIF_USBDI_VERSION_0:
		pInterfaceV0 = (USB_BUS_INTERFACE_USBDI_V0*)pIRP_Stack->Parameters.QueryInterface.Interface;
		break;
	case USB_BUSIF_USBDI_VERSION_1:
		pInterfaceV1 = (USB_BUS_INTERFACE_USBDI_V1*)pIRP_Stack->Parameters.QueryInterface.Interface;
		pInterfaceV0 = (USB_BUS_INTERFACE_USBDI_V0*)pInterfaceV1;
		break;
	case USB_BUSIF_USBDI_VERSION_2:
		pInterfaceV2 = (USB_BUS_INTERFACE_USBDI_V2*)pIRP_Stack->Parameters.QueryInterface.Interface;
		pInterfaceV0 = (USB_BUS_INTERFACE_USBDI_V0*)pInterfaceV2;
		pInterfaceV1 = (USB_BUS_INTERFACE_USBDI_V1*)pInterfaceV2;
		break;
	case USB_BUSIF_USBDI_VERSION_3:
		pInterfaceV3 = (USB_BUS_INTERFACE_USBDI_V3*)pIRP_Stack->Parameters.QueryInterface.Interface;
		pInterfaceV0 = (USB_BUS_INTERFACE_USBDI_V0*)pInterfaceV3;
		pInterfaceV1 = (USB_BUS_INTERFACE_USBDI_V1*)pInterfaceV3;
		pInterfaceV2 = (USB_BUS_INTERFACE_USBDI_V2*)pInterfaceV3;
		break;
	}//~hctiws
	
	// USBDI_#shared. backward compati.
	pInterfaceV1->BusContext = pPDO_DeviceData;
	pInterfaceV1->InterfaceReference   = (PINTERFACE_REFERENCE)   CB_InterfaceReference;
	pInterfaceV1->InterfaceDereference = (PINTERFACE_DEREFERENCE) CB_InterfaceDereference;

	switch (pIRP_Stack->Parameters.QueryInterface.Version) {
	case USB_BUSIF_USBDI_VERSION_3:
		pInterfaceV3->QueryBusTimeEx		= CB_QueryBusTimeEx;
		pInterfaceV3->QueryControllerType	= CB_QueryControllerType;
		// pass to lower version...
	case USB_BUSIF_USBDI_VERSION_2:
		pInterfaceV2->EnumLogEntry			= CB_EnumLogEntry;
		// pass to lower version...
	case USB_BUSIF_USBDI_VERSION_1:
		pInterfaceV1->IsDeviceHighSpeed		= CB_IsDeviceHighSpeed;
		// pass to lower version...
	case USB_BUSIF_USBDI_VERSION_0:
		pInterfaceV0->QueryBusInformation	= CB_QueryBusInformation;
		pInterfaceV0->SubmitIsoOutUrb		= CB_SubmitIsoOutUrb;
		pInterfaceV0->QueryBusTime			= CB_QueryBusTime;
		pInterfaceV0->GetUSBDIVersion		= CB_GetUSBDIVersion;
		break;
	}//~htctiws
	
	CB_InterfaceReference(pPDO_DeviceData);
	return (status);
}

