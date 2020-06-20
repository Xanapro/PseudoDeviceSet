#include "./busenum.h"
#include "./ntmiscdbg.h"


#include "../shared_headers/resolvetypes.h"
#if __has_include("../shared_headers/sha512.h")
	#include "../shared_headers/sha512.h"
	#include "../shared_headers/sha512.c"
#else // | __has_include("../shared_headers/sha512.h")
	int sha512_hash(char* pTo, U32 dstLen, const char* const pSrc, U32 len) {
		UNREFERENCED_PARAMETER(pTo);
		UNREFERENCED_PARAMETER(dstLen);
		UNREFERENCED_PARAMETER(pSrc);
		UNREFERENCED_PARAMETER(len);
		return (0);
	}
#endif//~#if __has_include("../shared_headers/sha512.h")




// sa: https://docs.microsoft.com/en-us/windows-hardware/drivers/devtest/using-static-driver-verifier-to-find-defects-in-drivers#preparing-your-source-code

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, Bus_EvtDeviceAdd)
#pragma alloc_text(PAGE, Bus_PnP)
#pragma alloc_text(PAGE, Bus_InitializePDO)
#pragma alloc_text(PAGE, bus_DestroyPDO)
#pragma alloc_text(PAGE, Bus_RemoveFdo)
#pragma alloc_text(PAGE, bus_FDO_PnP)
#pragma alloc_text(PAGE, Bus_StartFdo)
#pragma alloc_text(PAGE, Bus_SendIrpSynchronously)

#pragma alloc_text(PAGE, protocol_bus_PlugInDevice)
#pragma alloc_text(PAGE, protocol_bus_UnplugDevice)
#endif//~defined(ALLOC_PRAGMA)


/////////////////////////////////////////////////
// Driver INSTALL
NTSTATUS Bus_EvtDeviceAdd(__in PDRIVER_OBJECT pDriverObject, __in PDEVICE_OBJECT pPDO)
{
	fstr("+++ Add Device: 0x%016llX\n", pPDO);
	//KdBreakPoint();

	NTSTATUS			status			= STATUS_SUCCESS;
	DEVICE_OBJECT*		pDeviceObject	= NULL;
	t_FDO_DeviceData*	pFDO_DeviceData	= NULL;
	PAGED_CODE();
	//
	//////////////////////////////////////////////

	status = IoCreateDevice(
		pDriverObject
		, sizeof (t_FDO_DeviceData)
		, NULL
		, FILE_DEVICE_BUS_EXTENDER
		, FILE_DEVICE_SECURE_OPEN
		, TRUE
		, &pDeviceObject);

	if (!NT_SUCCESS(status)) {
		fstr("___ IoCreateDevice() failure!!! ret: 0x%08X(%d)", status, status);
		goto End;
	}

	pFDO_DeviceData = (t_FDO_DeviceData*)pDeviceObject->DeviceExtension;
	memset(pFDO_DeviceData, 0, sizeof (t_FDO_DeviceData));

	// pDevieData <> pPDO_DeviceData
	pFDO_DeviceData->m_eDevicePnPState		= E_DPNP_NOT_STARTED;
	pFDO_DeviceData->m_ePreviousPnPState	= E_DPNP_NOT_STARTED;
	pFDO_DeviceData->m_bIsFDO = TRUE;
	pFDO_DeviceData->m_pSelfDeviceObj = pDeviceObject;

	ExInitializeFastMutex(&pFDO_DeviceData->m_Mutex);
	InitializeListHead(&pFDO_DeviceData->m_lstentListOfPDOs);

	pFDO_DeviceData->m_pUnderlyingPDO = pPDO;

	pFDO_DeviceData->m_eDevicePowerState = PowerDeviceUnspecified;
	pFDO_DeviceData->m_eSystemPowerState = PowerSystemWorking;
	pFDO_DeviceData->m_ulOutstandingIO = 1;

	KeInitializeEvent(&pFDO_DeviceData->m_evtRemove, SynchronizationEvent, FALSE);
	KeInitializeEvent(&pFDO_DeviceData->m_evtStop, SynchronizationEvent, TRUE);

	pDeviceObject->Flags |= DO_POWER_PAGABLE;

	status = IoRegisterDeviceInterface(pPDO, (LPGUID)&GUID_DEVINTERFACE_PSEUDOBUS, NULL, &pFDO_DeviceData->m_ucInterfaceName);

	if (!NT_SUCCESS(status)) {
		fstr("___ IoRegisterDeviceInterface() failure!!! ret: 0x%08X(%d)", status, status);
		goto End;
	}

	pFDO_DeviceData->m_pNextLowerDriver = IoAttachDeviceToDeviceStack(pDeviceObject, pPDO);

	if (NULL == pFDO_DeviceData->m_pNextLowerDriver) {
		status = STATUS_NO_SUCH_DEVICE;
		goto End;
	}

	pDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

End:
	if (!NT_SUCCESS(status) && NULL != pDeviceObject) {
		fstr("___ IoCreateDevice() / IoRegisterDeviceInterface() failure!!! ret: 0x%08X(%d), pDeviceObject: 0x%016llX\n", status, status, pDeviceObject);
		if (NULL != pFDO_DeviceData && pFDO_DeviceData->m_pNextLowerDriver) {
			IoDetachDevice(pFDO_DeviceData->m_pNextLowerDriver);
		}
		IoDeleteDevice(pDeviceObject);
	}
	
	return (status);
}

NTSTATUS Bus_PnP(DEVICE_OBJECT* pDeviceObject, PIRP pIRP)
{
	NTSTATUS			status	= STATUS_SUCCESS;
	IO_STACK_LOCATION*	pIRP_Stack;
	t_Cmn_DeviceData*	pCmn_DeviceData	= NULL;
	PAGED_CODE();
	//
	//////////////////////////////////////////////


	pIRP_Stack		= IoGetCurrentIrpStackLocation(pIRP);
	ASSERT(IRP_MJ_PNP == pIRP_Stack->MajorFunction);

	pCmn_DeviceData = (t_Cmn_DeviceData*)pDeviceObject->DeviceExtension;

	if (E_DPNP_DELETED == pCmn_DeviceData->m_eDevicePnPState) {
		pIRP->IoStatus.Status = status = STATUS_NO_SUCH_DEVICE;
		IoCompleteRequest(pIRP, IO_NO_INCREMENT);
		return (status);
	}

	if (pCmn_DeviceData->m_bIsFDO) {
		fstr("+++ FDO %s IRP: 0x%016llX\n", PnPMinorFunctionString(pIRP_Stack->MinorFunction), pIRP);
		status = bus_FDO_PnP(pDeviceObject, pIRP, pIRP_Stack, (t_FDO_DeviceData*)pCmn_DeviceData);
	}
	else {
		fstr("+++ PDO %s IRP: 0x%016llX\n", PnPMinorFunctionString(pIRP_Stack->MinorFunction), pIRP);
		status = Bus_PDO_PnP(pDeviceObject, pIRP, pIRP_Stack, (t_PDO_DeviceData*)pCmn_DeviceData);
	}

	return (status);
}

NTSTATUS bus_FDO_PnP(__in PDEVICE_OBJECT pDeviceObject, __in PIRP pIRP, __in IO_STACK_LOCATION* pIRP_Stack, __in t_FDO_DeviceData* pFDO_DeviceData)
{
	NTSTATUS			status	= STATUS_SUCCESS;
	ULONG				ulLength	= 0;
	ULONG				ulPrevcount		= 0;
	ULONG				ulNumPdosPresent	= 0;
	ULONG				ulNumPdosMissing	= 0;
	LIST_ENTRY*			pEnt		= NULL;
	LIST_ENTRY*			pEntHead	= NULL;
	LIST_ENTRY*			pEntNext	= NULL;
	t_PDO_DeviceData*	pPDO_DeviceData		= NULL;
	DEVICE_RELATIONS*	pDevRelations		= NULL;
	DEVICE_RELATIONS*	pOldDevRelations	= NULL;
	PAGED_CODE();
	//
	//////////////////////////////////////////////

	bus_AddRef(pFDO_DeviceData);

	switch (pIRP_Stack->MinorFunction) {
	case IRP_MN_START_DEVICE:
	{
		status = Bus_SendIrpSynchronously(pFDO_DeviceData->m_pNextLowerDriver, pIRP);
		if (NT_SUCCESS(status)) {
			status = Bus_StartFdo(pFDO_DeviceData, pIRP);
		}

		pIRP->IoStatus.Status = status;
		IoCompleteRequest(pIRP, IO_NO_INCREMENT);

		bus_ReleaseRef(pFDO_DeviceData);
		return (status);
	}
	break;

	case IRP_MN_QUERY_STOP_DEVICE:
	{
		SET_NEW_PNP_STATE(pFDO_DeviceData, E_DPNP_STOP_PENDING);
		//pFDO_DeviceData->m_ePreviousPnPState = pFDO_DeviceData->m_eDevicePnPState;
		//pFDO_DeviceData->m_eDevicePnPState = E_DPNP_STOP_PENDING;
		pIRP->IoStatus.Status = STATUS_SUCCESS;
	}
	break;

	case IRP_MN_CANCEL_STOP_DEVICE:
	{
		if (E_DPNP_STOP_PENDING == pFDO_DeviceData->m_eDevicePnPState) {
			RESTORE_PREVIOUS_PNP_STATE(pFDO_DeviceData);//pFDO_DeviceData->m_eDevicePnPState = pFDO_DeviceData->m_ePreviousPnPState;
			if (E_DPNP_STARTED != pFDO_DeviceData->m_eDevicePnPState) {
				fstr("___ E_DPNP_STARTED != pFDO_DeviceData->m_eDevicePnPState\n");
				////////pIRP->IoStatus.Status = STATUS_SUCCESS;
			}//~fi
		}//~f
		pIRP->IoStatus.Status = STATUS_SUCCESS;
	}
	break;

	case IRP_MN_STOP_DEVICE:
	{
		bus_ReleaseRef(pFDO_DeviceData);
		KeWaitForSingleObject(&pFDO_DeviceData->m_evtStop, Executive, KernelMode, FALSE, NULL);
		bus_AddRef(pFDO_DeviceData);
		SET_NEW_PNP_STATE(pFDO_DeviceData, E_DPNP_STOPPED);
		//pFDO_DeviceData->m_ePreviousPnPState = pFDO_DeviceData->m_eDevicePnPState;
		//pFDO_DeviceData->m_eDevicePnPState = E_DPNP_STOPPED;

		pIRP->IoStatus.Status = STATUS_SUCCESS;
	}
	break;

	case IRP_MN_QUERY_REMOVE_DEVICE:
	{
		SET_NEW_PNP_STATE(pFDO_DeviceData, E_DPNP_REMOVE_PENDING);
		//pFDO_DeviceData->m_ePreviousPnPState = pFDO_DeviceData->m_eDevicePnPState;
		//pFDO_DeviceData->m_eDevicePnPState = E_DPNP_REMOVE_PENDING;
		pIRP->IoStatus.Status = STATUS_SUCCESS;
	}
	break;

	case IRP_MN_CANCEL_REMOVE_DEVICE:
	{
		if (E_DPNP_REMOVE_PENDING == pFDO_DeviceData->m_eDevicePnPState) {
			RESTORE_PREVIOUS_PNP_STATE(pFDO_DeviceData);
			///pFDO_DeviceData->m_eDevicePnPState = pFDO_DeviceData->m_ePreviousPnPState;
		}
		pIRP->IoStatus.Status = STATUS_SUCCESS;
	}
	break;

	case IRP_MN_SURPRISE_REMOVAL:
	{
		// sa: https://docs.microsoft.com/en-us/windows-hardware/drivers/kernel/handling-an-irp-mn-surprise-removal-request
		SET_NEW_PNP_STATE(pFDO_DeviceData, E_DPNP_SURPRISE_REMOVE_PENDING);
		//pFDO_DeviceData->m_ePreviousPnPState = pFDO_DeviceData->m_eDevicePnPState;
		//pFDO_DeviceData->m_eDevicePnPState = E_DPNP_SURPRISE_REMOVE_PENDING;

		Bus_RemoveFdo(pFDO_DeviceData);

ExAcquireFastMutex(&pFDO_DeviceData->m_Mutex);
		{
			pEntHead = &pFDO_DeviceData->m_lstentListOfPDOs;

			for (pEnt = pEntHead->Flink, pEntNext = pEnt->Flink; pEnt != pEntHead; pEnt = pEntNext, pEntNext = pEnt->Flink) {
				pPDO_DeviceData = CONTAINING_RECORD(pEnt, t_PDO_DeviceData, Link);

				RemoveEntryList(&pPDO_DeviceData->Link);
				InitializeListHead(&pPDO_DeviceData->Link);

				pPDO_DeviceData->m_pParentFDO = NULL;
				pPDO_DeviceData->m_bIsReportedMissing = TRUE;
			}//~rof
		}
ExReleaseFastMutex(&pFDO_DeviceData->m_Mutex);

		pIRP->IoStatus.Status = STATUS_SUCCESS;
	}//~case IRP_MN_SURPRISE_REMOVAL:
	break;

	case IRP_MN_REMOVE_DEVICE:
	{
		if (E_DPNP_SURPRISE_REMOVE_PENDING != pFDO_DeviceData->m_eDevicePnPState) {
			Bus_RemoveFdo(pFDO_DeviceData);
		}

		SET_NEW_PNP_STATE(pFDO_DeviceData, E_DPNP_DELETED);///	migration? sa: ./tools/dv/samples/DV-FailDriver-WDM/defect_toastmon.h
		//pFDO_DeviceData->m_ePreviousPnPState	= pFDO_DeviceData->m_eDevicePnPState;
		//pFDO_DeviceData->m_eDevicePnPState		= E_DPNP_DELETED;

#if 1
		// OK. force ref cnt to 0
		bus_ReleaseRef(pFDO_DeviceData);
		bus_ReleaseRef(pFDO_DeviceData);
		
		/////bus_ReleaseRef(pFDO_DeviceData);
		/////bus_ReleaseRef(pFDO_DeviceData);
#else // |
		//!!! problem when uninstalling the device.
		// 
		///bus_ReleaseRef(pFDO_DeviceData);	//!!! why
		bus_ReleaseRef(pFDO_DeviceData);
#endif//~if0

		KeWaitForSingleObject(&pFDO_DeviceData->m_evtRemove, Executive, KernelMode, FALSE, NULL);
ExAcquireFastMutex(&pFDO_DeviceData->m_Mutex);
		{
			pEntHead = &pFDO_DeviceData->m_lstentListOfPDOs;

			for (pEnt = pEntHead->Flink, pEntNext = pEnt->Flink; pEnt != pEntHead; pEnt = pEntNext, pEntNext = pEnt->Flink) {
				pPDO_DeviceData = CONTAINING_RECORD(pEnt, t_PDO_DeviceData, Link);
				RemoveEntryList(&pPDO_DeviceData->Link);

				if (E_DPNP_SURPRISE_REMOVE_PENDING == pPDO_DeviceData->m_eDevicePnPState) {
					fstr("+++ \t Found a surprise removed device: 0x%016LLX\n", pPDO_DeviceData->m_pSelfDeviceObj);
					InitializeListHead(&pPDO_DeviceData->Link);
					pPDO_DeviceData->m_pParentFDO			= NULL;
					pPDO_DeviceData->m_bIsReportedMissing	= TRUE;
					continue;
				}//~fi
				///bus_DestroyPDO(pPDO_DeviceData->m_pSelfDeviceObj, pPDO_DeviceData);
				bus_DestroyPDO(pPDO_DeviceData->m_pSelfDeviceObj->DeviceExtension, pPDO_DeviceData);
				fstr("+++ \t Deleting PDO: 0x%016LLX\n", pPDO_DeviceData->m_pSelfDeviceObj);
				IoDeleteDevice( __drv_freesMem(mem) pPDO_DeviceData->m_pSelfDeviceObj);
			}//~rofnn
		}
ExReleaseFastMutex(&pFDO_DeviceData->m_Mutex);

		pIRP->IoStatus.Status = STATUS_SUCCESS;
		IoSkipCurrentIrpStackLocation(pIRP);

		status = IoCallDriver(pFDO_DeviceData->m_pNextLowerDriver, pIRP);
		IoDetachDevice(pFDO_DeviceData->m_pNextLowerDriver);
		fstr("+++ \t Deleting FDO: 0x%016LLX\n", pDeviceObject);

		IoDeleteDevice(pDeviceObject);
		return (status);
	}//~case IRP_MN_REMOVE_DEVICE:
	break;

	case IRP_MN_QUERY_DEVICE_RELATIONS:
	{
		fstr("+++ \t QueryDeviceRelation Type: %s\n", DbgDeviceRelationString(pIRP_Stack->Parameters.QueryDeviceRelations.Type));
		if (BusRelations != pIRP_Stack->Parameters.QueryDeviceRelations.Type) {
			break;
		}
ExAcquireFastMutex(&pFDO_DeviceData->m_Mutex);
		pOldDevRelations = (DEVICE_RELATIONS*)pIRP->IoStatus.Information;
		if (NULL != pOldDevRelations) {
			ulPrevcount = pOldDevRelations->Count;
			if (0 == pFDO_DeviceData->m_ulNumPDOs) {
ExReleaseFastMutex(&pFDO_DeviceData->m_Mutex);
				break;
			}
		}
		else {
			ulPrevcount = 0;
		}

		ulNumPdosPresent = 0;
		ulNumPdosMissing = 0;

		for (pEnt = pFDO_DeviceData->m_lstentListOfPDOs.Flink; pEnt != &pFDO_DeviceData->m_lstentListOfPDOs; pEnt = pEnt->Flink) {
			pPDO_DeviceData = CONTAINING_RECORD(pEnt, t_PDO_DeviceData, Link);
			if (pPDO_DeviceData->m_bIsPresent) {
				ulNumPdosPresent++;
			}
		}//~rof

		ulLength = sizeof (DEVICE_RELATIONS) + ((ulNumPdosPresent + ulPrevcount) * sizeof (DEVICE_OBJECT*)) - 1;
		pDevRelations = (DEVICE_RELATIONS*)ExAllocatePoolWithTag(PagedPool, ulLength, K_BUSENUM_POOL_TAG);
		if (NULL == pDevRelations) {
ExReleaseFastMutex(&pFDO_DeviceData->m_Mutex);

			pIRP->IoStatus.Status = status = STATUS_INSUFFICIENT_RESOURCES;
			IoCompleteRequest(pIRP, IO_NO_INCREMENT);

			bus_ReleaseRef(pFDO_DeviceData);
			return (status);
		}//~fi

		if (ulPrevcount) {
			RtlCopyMemory(pDevRelations->Objects, pOldDevRelations->Objects, ulPrevcount * sizeof (DEVICE_OBJECT*));
		}

		pDevRelations->Count = ulPrevcount + ulNumPdosPresent;

		for (pEnt = pFDO_DeviceData->m_lstentListOfPDOs.Flink; pEnt != &pFDO_DeviceData->m_lstentListOfPDOs; pEnt = pEnt->Flink) {
			pPDO_DeviceData = CONTAINING_RECORD(pEnt, t_PDO_DeviceData, Link);
			if (pPDO_DeviceData->m_bIsPresent) {
				pDevRelations->Objects[ulPrevcount] = pPDO_DeviceData->m_pSelfDeviceObj;
				ObReferenceObject(pPDO_DeviceData->m_pSelfDeviceObj);
				ulPrevcount++;
			}
			else {
				pPDO_DeviceData->m_bIsReportedMissing = TRUE;
				ulNumPdosMissing++;
			}
		}//~rof

		fstr("++# PDOS Present = %d, Reported = %d, Missing = %d, Listed = %d\n", ulNumPdosPresent, pDevRelations->Count, ulNumPdosMissing, pFDO_DeviceData->m_ulNumPDOs);

		if (pOldDevRelations) {
			ExFreePool(pOldDevRelations);
		}

		pIRP->IoStatus.Information = (ULONG_PTR)pDevRelations;

ExReleaseFastMutex(&pFDO_DeviceData->m_Mutex);

		pIRP->IoStatus.Status = STATUS_SUCCESS;
	}//~case IRP_MN_QUERY_DEVICE_RELATIONS:
	break;

	default:
		break;
	}//~hctiws

	IoSkipCurrentIrpStackLocation(pIRP);
	status = IoCallDriver(pFDO_DeviceData->m_pNextLowerDriver, pIRP);

	bus_ReleaseRef(pFDO_DeviceData);
	return (status);
}

NTSTATUS Bus_StartFdo(__in t_FDO_DeviceData* pFDO_DeviceData, __in IRP* pIRP)
{
	UNREFERENCED_PARAMETER(pIRP);
	
	NTSTATUS		status				= STATUS_SUCCESS;
	PWSTR			pSymbolicNameList	= NULL;
	POWER_STATE		powerState;

	PAGED_CODE();
	//
	//////////////////////////////////////////////

	//// Test that there isn't another such bus
	status = IoGetDeviceInterfaces(&GUID_DEVINTERFACE_PSEUDOBUS, NULL, 0/*DEVICE_INTERFACE_INCLUDE_NONACTIVE*/, &pSymbolicNameList);
	if (0 != *pSymbolicNameList) {
		fstr("+++ Add Device (bus): Already exists\n");
		return (STATUS_NO_SUCH_DEVICE);
	}

	status = IoSetDeviceInterfaceState(&pFDO_DeviceData->m_ucInterfaceName, TRUE);
	if (!NT_SUCCESS(status)) {
		fstr("___ IoSetDeviceInterfaceState failed: 0x%x\n", status);
		return (status);
	}

	//////////////////////////////////////////////
	//
	pFDO_DeviceData->m_eDevicePowerState	= PowerDeviceD0;
	powerState.DeviceState					= PowerDeviceD0;

	PoSetPowerState(pFDO_DeviceData->m_pSelfDeviceObj, DevicePowerState, powerState);
	SET_NEW_PNP_STATE(pFDO_DeviceData, E_DPNP_STARTED);
	//pFDO_DeviceData->m_ePreviousPnPState	= pFDO_DeviceData->m_eDevicePnPState;
	//pFDO_DeviceData->m_eDevicePnPState		= E_DPNP_STARTED;

	return (status);
}

VOID Bus_RemoveFdo(__in t_FDO_DeviceData* pFDO_DeviceData)
{
	PAGED_CODE();
	//
	//////////////////////////////////////////////

	fstr("+++ CALLED\n");
	if (pFDO_DeviceData->m_ucInterfaceName.Buffer != NULL) {
		IoSetDeviceInterfaceState(&pFDO_DeviceData->m_ucInterfaceName, FALSE);

		ExFreePool(pFDO_DeviceData->m_ucInterfaceName.Buffer);
		RtlZeroMemory(&pFDO_DeviceData->m_ucInterfaceName, sizeof (UNICODE_STRING));
	}
}

NTSTATUS Bus_SendIrpSynchronously(__in DEVICE_OBJECT* pDeviceObject, __in IRP* pIRP)
{
	NTSTATUS	status = STATUS_SUCCESS;
	KEVENT		evt;
	PAGED_CODE();
	//
	//////////////////////////////////////////////

	KeInitializeEvent(&evt, NotificationEvent, FALSE);
	IoCopyCurrentIrpStackLocationToNext(pIRP);
	IoSetCompletionRoutine(pIRP, Bus_CompletionRoutine, &evt, TRUE, TRUE, TRUE);
	
	status = IoCallDriver(pDeviceObject, pIRP);
	if (STATUS_PENDING == status) {
		KeWaitForSingleObject(&evt, Executive, KernelMode, FALSE, NULL);
		status = pIRP->IoStatus.Status;
	}//~

	return (status);
}

NTSTATUS Bus_CompletionRoutine(DEVICE_OBJECT* pDeviceObject, IRP* pIRP, PVOID pvContext)
{
	UNREFERENCED_PARAMETER(pDeviceObject);
	//
	//////////////////////////////////////////////

	if (pIRP->PendingReturned) {
		KeSetEvent((PKEVENT)pvContext, IO_NO_INCREMENT, FALSE);
	}

	return (STATUS_MORE_PROCESSING_REQUIRED);
}

NTSTATUS bus_DestroyPDO(__drv_in(__drv_aliasesMem) t_FDO_DeviceData* pFDO_DeviceData, t_PDO_DeviceData* pPDO_Data)
{
	PAGED_CODE();
	//
	//////////////////////////////////////////////
	fstr("+++ CALLED\n");

	if (NULL == pFDO_DeviceData) {
		fstr("___ !!! NULL == pFDO_DeviceData !!!\n");
	}
///ExAcquireFastMutex(&pFDO_DeviceData->m_Mutex);
	pFDO_DeviceData->m_ulNumPDOs--;
///ExReleaseFastMutex(&pFDO_DeviceData->m_Mutex);

	if (NULL != pPDO_Data->m_ucInterfaceName.Buffer) {
		ExFreePool(pPDO_Data->m_ucInterfaceName.Buffer);
		RtlZeroMemory(&pPDO_Data->m_ucInterfaceName, sizeof (UNICODE_STRING));
	}

	// cleanup
	if (NULL != pPDO_Data->m_puniHardwareID) {
		ExFreePool(pPDO_Data->m_puniHardwareID);		//@@@
		pPDO_Data->m_puniHardwareID = NULL;
	}

	//fstr("\tDeleting PDO: 0x%016llX\n", pDeviceObject);
	///IoDeleteDevice(pDeviceObject);

	return (STATUS_SUCCESS);
}

////////////////////////////////////////////
//!!!
VOID Bus_InitializePDO(__drv_in(__drv_aliasesMem) DEVICE_OBJECT* pPDO_DeviceObject, t_FDO_DeviceData* pFDO_DeviceData)
{
	t_PDO_DeviceData*	pPDO_DeviceData = NULL;
	PAGED_CODE();
	//
	//////////////////////////////////////////////

	pPDO_DeviceData = (t_PDO_DeviceData*)pPDO_DeviceObject->DeviceExtension;

	fstr("+++ PDO 0x%016llX, Extension 0x%016llX\n", pPDO_DeviceObject, pPDO_DeviceData);

	pPDO_DeviceData->m_pSelfDeviceObj		= pPDO_DeviceObject;
	pPDO_DeviceData->m_dwCallingPID			= 0x00000000;
	memset(pPDO_DeviceData->m_byIndividualCode, 0, array_sizeof(pPDO_DeviceData->m_byIndividualCode));
	pPDO_DeviceData->m_pParentFDO			= pFDO_DeviceData->m_pSelfDeviceObj;
	pPDO_DeviceData->m_bIsPresent			= TRUE;
	pPDO_DeviceData->m_bIsFDO				= FALSE;
	pPDO_DeviceData->m_bIsStarted			= FALSE;
	pPDO_DeviceData->m_bIsReportedMissing	= FALSE;

	pPDO_DeviceData->m_eDevicePnPState		= E_DPNP_NOT_STARTED;
	pPDO_DeviceData->m_ePreviousPnPState	= E_DPNP_NOT_STARTED;
	pPDO_DeviceData->m_eDevicePowerState	= PowerDeviceD3;
	pPDO_DeviceData->m_eSystemPowerState	= PowerSystemWorking;
	pPDO_DeviceObject->Flags |= DO_POWER_PAGABLE;		//!!! DONT mv !!! pre m_pSelfDeviceObj

	KeInitializeSpinLock(&pPDO_DeviceData->m_lstentPendingQueueLock);
	InitializeListHead(&pPDO_DeviceData->m_lstentPendingQueue);
	InitializeListHead(&pPDO_DeviceData->m_lstentHoldingQueue);

ExAcquireFastMutex(&pFDO_DeviceData->m_Mutex);
	{
		InsertTailList(&pFDO_DeviceData->m_lstentListOfPDOs, &pPDO_DeviceData->Link);
		pFDO_DeviceData->m_ulNumPDOs++;
	}
ExReleaseFastMutex(&pFDO_DeviceData->m_Mutex);

	pPDO_DeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;
}


NTSTATUS protocol_bus_PlugInDevice(t_BusEnumPluginReq* pReq, t_FDO_DeviceData* pFDO_DeviceData, ULONG ulOutputLen, IRP* pIRP)
{
	UNREFERENCED_PARAMETER(ulOutputLen);
	UNREFERENCED_PARAMETER(pIRP);
	NTSTATUS			status	= STATUS_SUCCESS;
	BOOLEAN				bUnique	= TRUE;
	LIST_ENTRY*			pEnt	= NULL;
	DEVICE_OBJECT*		pPDO_DeviceObject	= NULL;
	t_PDO_DeviceData*	pPDO_DeviceData		= NULL;
	char				szHash[132];
	int					iret = 0;
	PAGED_CODE();
	//
	////////////////////////////////////////////

	fstr("+++ Exposing PDO: ====== Serial: %d, Device: %ws, Length: %d\n", pReq->byIdx, K_DEVICE_HARDWARE_ID, sizeof (K_DEVICE_HARDWARE_ID));

	memset(szHash, 0, array_sizeof (szHash));
	///iret	= sha512_hash(szHash, array_sizeof (szHash), (char*)pReq->byIndividualCode, array_sizeof (pReq->byIndividualCode));
	iret = sha512_hash(szHash, array_sizeof(szHash), (char*)pReq->byaKey, (U32)strlen((char*)pReq->byaKey));
	if (0 > iret) {
		fstr("___ sha512_hash() failure!!! ret: %d\n", iret);
		return (STATUS_INVALID_DEVICE_REQUEST);
	}
	if (0 == strncmp(szHash, K_INDIVIDUAL_HASH, strlen(szHash))) {
	///	fstr("+++ K: %.128s,   VALID_HASH: %s\n", (char*)pReq->byaKey, szHash);
	}
	else {
	///	fstr("___ K: %.128s, INVALID_HASH: %s\n", (char*)pReq->byaKey, szHash);
		return (STATUS_INVALID_DEVICE_REQUEST);
	}
	RtlSecureZeroMemory(szHash, array_sizeof (szHash));


ExAcquireFastMutex(&pFDO_DeviceData->m_Mutex);
	{
		for (pEnt = pFDO_DeviceData->m_lstentListOfPDOs.Flink; pEnt != &pFDO_DeviceData->m_lstentListOfPDOs; pEnt = pEnt->Flink) {
			pPDO_DeviceData = CONTAINING_RECORD(pEnt, t_PDO_DeviceData, Link);
			if (pReq->byIdx == pPDO_DeviceData->m_ulPadIdx
				&& E_DPNP_SURPRISE_REMOVE_PENDING != pPDO_DeviceData->m_eDevicePnPState) {
				bUnique = FALSE;
				break;
			}
		}//~rof
	}
ExReleaseFastMutex(&pFDO_DeviceData->m_Mutex);

	if (!bUnique || 0 == pReq->byIdx) {
		fstr("___ INVALID PARAM idx: %d\n", pReq->byIdx);
		return (STATUS_INVALID_PARAMETER);
	}

	fstr("+++ pFDO_DeviceData->m_pNextLowerDriver = 0x%016llX\n", pFDO_DeviceData->m_pNextLowerDriver);
	// sa: http://www.osronline.com/article.cfm%5Eid=105.htm
	status = IoCreateDeviceSecure(
		pFDO_DeviceData->m_pSelfDeviceObj->DriverObject,
		sizeof(t_PDO_DeviceData),
		NULL,
		FILE_DEVICE_BUS_EXTENDER,
		FILE_AUTOGENERATED_DEVICE_NAME | FILE_DEVICE_SECURE_OPEN,
		FALSE,
		&SDDL_DEVOBJ_SYS_ALL_ADM_RWX_WORLD_RWX_RES_RWX,
		(LPCGUID)&GUID_DEVCLASS_PSEUDO_BUS_DEVICE,
		&pPDO_DeviceObject
	);

	if (!NT_SUCCESS(status)) {
		///t_BusEnumPluginRep rep;
		///rep.byState = 250;
		///status = irpcopy(pIRP, &rep, sizeof(t_BusEnumPluginRep), ulOutputLen);
		return (status);
	}//~

	pPDO_DeviceData = (t_PDO_DeviceData*)pPDO_DeviceObject->DeviceExtension;
	pPDO_DeviceData->m_puniHardwareID = ExAllocatePoolWithTag(NonPagedPool, sizeof (K_DEVICE_HARDWARE_ID), K_BUSENUM_POOL_TAG);
	if (NULL == pPDO_DeviceData->m_puniHardwareID) {
		IoDeleteDevice(pPDO_DeviceObject);		// !! DONT FORGET !!
		///t_BusEnumPluginRep rep;
		///rep.byState = 240;
		///status = irpcopy(pIRP, &rep, sizeof(t_BusEnumPluginRep), ulOutputLen);
		return (STATUS_INSUFFICIENT_RESOURCES);
	}//~

	RtlCopyMemory(pPDO_DeviceData->m_puniHardwareID, K_DEVICE_HARDWARE_ID, sizeof (K_DEVICE_HARDWARE_ID));

	pPDO_DeviceData->m_ulPadIdx			= pReq->byIdx;
	Bus_InitializePDO(pPDO_DeviceObject, pFDO_DeviceData);

	pPDO_DeviceData->m_dwCallingPID	= (DWORD)(DWORD_PTR)PsGetCurrentProcessId();	// get caller pid.
	//memcpy_s(pPDO_DeviceData->m_byIndividualCode, array_sizeof(pPDO_DeviceData->m_byIndividualCode), pReq->byIndividualCode, array_sizeof(pReq->byIndividualCode));
	memcpy(pPDO_DeviceData->m_byIndividualCode, pReq->byIndividualCode, array_sizeof(pReq->byIndividualCode));

	fstr("+++ app_pid: %u\n", pPDO_DeviceData->m_dwCallingPID);

	IoInvalidateDeviceRelations(pFDO_DeviceData->m_pUnderlyingPDO, BusRelations);

	///t_BusEnumPluginRep rep;
	///rep.byState = 1;
	///irpcopy(pIRP, &rep, sizeof(t_BusEnumPluginRep), ulOutputLen);

	return (status);
}

NTSTATUS protocol_bus_UnplugDevice(t_BusEnumUnplugReq* pReq, t_FDO_DeviceData* pFDO_DeviceData, ULONG ulOutputLen, IRP* pIRP)
{
	UNREFERENCED_PARAMETER(ulOutputLen);
	UNREFERENCED_PARAMETER(pIRP);
	LIST_ENTRY*			pEnt	= NULL;
	t_PDO_DeviceData*	pPDO_DeviceData	= NULL;
	BOOLEAN             bDetected		= FALSE;
	PAGED_CODE();
	//
	/////////////////////////////////////

ExAcquireFastMutex(&pFDO_DeviceData->m_Mutex);
	{
		if (0x00 != pReq->byAll) {
			fstr("+++ %splugging out all the devices!\n", pReq->byForce ? "Force " : "" );
		}
		else {
			fstr("+++ %splugging out %d\n", pReq->byIdx, pReq->byForce ? "Force " : "");
		}

		if (0 == pFDO_DeviceData->m_ulNumPDOs) {
			fstr("#_# NO_SUCH_DEVICE...\n");
ExReleaseFastMutex(&pFDO_DeviceData->m_Mutex);
			return (STATUS_SUCCESS);///return (STATUS_NO_SUCH_DEVICE);
		}

		for (pEnt = pFDO_DeviceData->m_lstentListOfPDOs.Flink; pEnt != &pFDO_DeviceData->m_lstentListOfPDOs; pEnt = pEnt->Flink) {
			pPDO_DeviceData = CONTAINING_RECORD(pEnt, t_PDO_DeviceData, Link);

			//!!!
			fstr("+++ Found device %d, havePid: 0x%08X, callPid: 0x%08X\n", pPDO_DeviceData->m_ulPadIdx, pPDO_DeviceData->m_dwCallingPID, (DWORD)(DWORD_PTR)PsGetCurrentProcessId());

			// step1: have own pid? or forced?
			if ((0 == pReq->byForce) && (pPDO_DeviceData->m_dwCallingPID != (DWORD)(DWORD_PTR)PsGetCurrentProcessId())) {
				continue;
			}//~
			
			// step2: Counterfeit measures. call from same app?
			if (0 != memcmp(pPDO_DeviceData->m_byIndividualCode, pReq->byIndividualCode, array_sizeof(pPDO_DeviceData->m_byIndividualCode))) {
				DWORD dwCallerPID = (DWORD)(DWORD_PTR)PsGetCurrentProcessId();
				UNREFERENCED_PARAMETER(dwCallerPID);
				fstr("#_# INVALID CALLER!!! callPid: 0x%08X(%u), appCode: %c%c%c%c%c\n", dwCallerPID, dwCallerPID
					, pReq->byIndividualCode[0]
					, pReq->byIndividualCode[1]
					, pReq->byIndividualCode[2]
					, pReq->byIndividualCode[3]
					, pReq->byIndividualCode[4]
				);
			///	continue;
			}
			
			if (0x00 != pReq->byAll || pReq->byIdx == pPDO_DeviceData->m_ulPadIdx) {
				fstr("+++ Plluged out %d, havePid: 0x%08X, callPid: 0x%08X\n", pPDO_DeviceData->m_ulPadIdx, pPDO_DeviceData->m_dwCallingPID, (DWORD)(DWORD_PTR)PsGetCurrentProcessId());
				pPDO_DeviceData->m_dwCallingPID = 0x00000000;
				memset(pPDO_DeviceData->m_byIndividualCode, 0, array_sizeof(pPDO_DeviceData->m_byIndividualCode));

				pPDO_DeviceData->m_bIsPresent = FALSE;
				bDetected = TRUE;

				if (0x00 == pReq->byAll) {
					break;
				}
			}//~fi
		}//~rof
	}
ExReleaseFastMutex(&pFDO_DeviceData->m_Mutex);

	if (bDetected) {
		// IRP_MJ_PNP
		IoInvalidateDeviceRelations(pFDO_DeviceData->m_pUnderlyingPDO, BusRelations);
		///t_BusEnumUnplugRep rep;
		return (STATUS_SUCCESS);
	}

	fstr("#_# Device %d is not present\n", pReq->byIdx);
	return (STATUS_SUCCESS);
	//return (STATUS_NO_SUCH_DEVICE);		// recorded log in EvtViewer
	//return (STATUS_INVALID_PARAMETER);
}
PCHAR PnPMinorFunctionString(UCHAR byMinorFunction)
{
	switch (byMinorFunction) {
	case IRP_MN_START_DEVICE:			return ("IRP_MN_START_DEVICE");
	case IRP_MN_QUERY_REMOVE_DEVICE:	return ("IRP_MN_QUERY_REMOVE_DEVICE");
	case IRP_MN_REMOVE_DEVICE:			return ("IRP_MN_REMOVE_DEVICE");
	case IRP_MN_CANCEL_REMOVE_DEVICE:	return ("IRP_MN_CANCEL_REMOVE_DEVICE");
	case IRP_MN_STOP_DEVICE:			return ("IRP_MN_STOP_DEVICE");
	case IRP_MN_QUERY_STOP_DEVICE:		return ("IRP_MN_QUERY_STOP_DEVICE");
	case IRP_MN_CANCEL_STOP_DEVICE:		return ("IRP_MN_CANCEL_STOP_DEVICE");
	case IRP_MN_QUERY_DEVICE_RELATIONS:	return ("IRP_MN_QUERY_DEVICE_RELATIONS");
	case IRP_MN_QUERY_INTERFACE:		return ("IRP_MN_QUERY_INTERFACE");
	case IRP_MN_QUERY_CAPABILITIES:		return ("IRP_MN_QUERY_CAPABILITIES");
	case IRP_MN_QUERY_RESOURCES:		return ("IRP_MN_QUERY_RESOURCES");
	case IRP_MN_QUERY_RESOURCE_REQUIREMENTS:	return ("IRP_MN_QUERY_RESOURCE_REQUIREMENTS");
	case IRP_MN_QUERY_DEVICE_TEXT:		return ("IRP_MN_QUERY_DEVICE_TEXT");
	case IRP_MN_FILTER_RESOURCE_REQUIREMENTS:	return ("IRP_MN_FILTER_RESOURCE_REQUIREMENTS");
	case IRP_MN_READ_CONFIG:			return ("IRP_MN_READ_CONFIG");
	case IRP_MN_WRITE_CONFIG:			return ("IRP_MN_WRITE_CONFIG");
	case IRP_MN_EJECT:					return ("IRP_MN_EJECT");
	case IRP_MN_SET_LOCK:				return ("IRP_MN_SET_LOCK");
	case IRP_MN_QUERY_ID:				return ("IRP_MN_QUERY_ID");
	case IRP_MN_QUERY_PNP_DEVICE_STATE:	return ("IRP_MN_QUERY_PNP_DEVICE_STATE");
	case IRP_MN_QUERY_BUS_INFORMATION:	return ("IRP_MN_QUERY_BUS_INFORMATION");
	case IRP_MN_DEVICE_USAGE_NOTIFICATION:		return ("IRP_MN_DEVICE_USAGE_NOTIFICATION");
	case IRP_MN_SURPRISE_REMOVAL:		return ("IRP_MN_SURPRISE_REMOVAL");
	case IRP_MN_QUERY_LEGACY_BUS_INFORMATION:	return ("IRP_MN_QUERY_LEGACY_BUS_INFORMATION");
	}//~hctws
	return ("Unknown PNP minor function codes");
}

PCHAR DbgDeviceRelationString(__in DEVICE_RELATION_TYPE eDRT)
{
	switch (eDRT) {
	case BusRelations:			return ("BusRelations");
	case EjectionRelations:		return ("EjectionRelations");
	case RemovalRelations:		return ("RemovalRelations");
	case TargetDeviceRelation:	return ("TargetDeviceRelation");
	}//~hctiws
	return ("Unknown DEVICE_RELATION_TYPE");
}

PCHAR DbgDeviceIDString(BUS_QUERY_ID_TYPE eBQIT)
{
	switch (eBQIT) {
	case BusQueryDeviceID:		return ("BusQueryDeviceID");
	case BusQueryHardwareIDs:	return ("BusQueryHardwareIDs");
	case BusQueryCompatibleIDs:	return ("BusQueryCompatibleIDs");
	case BusQueryInstanceID:	return ("BusQueryInstanceID");
	case BusQueryDeviceSerialNumber:	return ("BusQueryDeviceSerialNumber");
	case BusQueryContainerID:	return ("BusQueryContainerID");
	}//~hctiws
	return ("Unknown BUS_QUERY_ID_TYPE");
}

