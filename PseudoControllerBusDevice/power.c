
#include "./busenum.h"
#include "./ntmiscdbg.h"

//sa https://docs.microsoft.com/en-us/windows-hardware/drivers/kernel/irp-mn-set-power
//sa https://docs.microsoft.com/en-us/windows-hardware/drivers/kernel/handling-device-power-down-irps

#pragma warning(disable : 4702)


NTSTATUS Bus_Power(DEVICE_OBJECT* pDeviceObject, PIRP pIRP)
{
	IO_STACK_LOCATION*	pIRPStack = IoGetCurrentIrpStackLocation(pIRP);
	fstr("+++ CALLED\n");

	if (NULL == pIRPStack) {
		fstr("\tNULL == pIRPStack !!! NOTHING REQUEST I/O PACKET !!!\n");
		return (STATUS_FWP_NULL_POINTER);
	}//~fi

	t_Cmn_DeviceData* pCmn_DeviceData = (t_Cmn_DeviceData*)pDeviceObject->DeviceExtension;
	NTSTATUS     status = STATUS_SUCCESS;

	switch (pCmn_DeviceData->m_eDevicePnPState) {
	case E_DPNP_DELETED:
	case E_DPNP_NOT_STARTED:
	case E_DPNP_STARTED:
	case E_DPNP_STOP_PENDING:
	case E_DPNP_STOPPED:
	case E_DPNP_REMOVE_PENDING:
	case E_DPNP_SURPRISE_REMOVE_PENDING:
	case E_DPNP_UNKNOWN:
	default:
		break;
	}//~hctiws

	if (E_DPNP_DELETED == pCmn_DeviceData->m_eDevicePnPState) {
		PoStartNextPowerIrp(pIRP);
		pIRP->IoStatus.Status	= STATUS_NO_SUCH_DEVICE ;
		status					= pIRP->IoStatus.Status;		// 
		IoCompleteRequest(pIRP, IO_NO_INCREMENT);
		return (status);
	}//`fi

	if (pCmn_DeviceData->m_bIsFDO) {
		fstr("+++ FDO %s IRP:0x%p %s %s\n"
			, powerMinorFunc2str(pIRPStack->MinorFunction)
			, pIRP
			, sysPowerState2str(pCmn_DeviceData->m_eSystemPowerState)
			, devPowerState2str(pCmn_DeviceData->m_eDevicePowerState)
		);

		status = bus_FDO_Wakeup((t_FDO_DeviceData*) pDeviceObject->DeviceExtension, pIRP);
	}//~fi
	else {
		fstr("+++ PDO %s IRP:0x%p %s %s\n", powerMinorFunc2str(pIRPStack->MinorFunction), pIRP, sysPowerState2str(pCmn_DeviceData->m_eSystemPowerState), devPowerState2str(pCmn_DeviceData->m_eDevicePowerState));

		status = bus_PDO_Wakeup((t_PDO_DeviceData*) pDeviceObject->DeviceExtension, pIRP);
	}//~esle

	return (status);
}

NTSTATUS bus_FDO_Wakeup(t_FDO_DeviceData* pFDO_DeviceData, PIRP pIRP)
{
	PIO_STACK_LOCATION  pIRPStack		= IoGetCurrentIrpStackLocation(pIRP);
	fstr("+++ CALLED\n");

	if (NULL == pIRPStack) {
		fstr("\tNULL == pIRPStack !!! NOTHING REQUEST I/O PACKET !!!\n");
		return (STATUS_FWP_NULL_POINTER);
	}//~fi
	POWER_STATE_TYPE	powerType	= pIRPStack->Parameters.Power.Type;
	POWER_STATE			powerState	= pIRPStack->Parameters.Power.State;
	UNREFERENCED_PARAMETER(powerType);
	UNREFERENCED_PARAMETER(powerState);

	bus_AddRef(pFDO_DeviceData);

	NTSTATUS            status		= STATUS_SUCCESS;
	if (E_DPNP_NOT_STARTED == pFDO_DeviceData->m_eDevicePnPState) {
		PoStartNextPowerIrp(pIRP);
		IoSkipCurrentIrpStackLocation(pIRP);
		// sa: https://docs.microsoft.com/en-us/windows-hardware/drivers/kernel/calling-iocalldriver-versus-calling-pocalldriver
		///status	= IoCallDriver(pFDO_DeviceData->m_pNextLowerDriver, pIRP);
		status	= PoCallDriver(pFDO_DeviceData->m_pNextLowerDriver, pIRP);

		bus_ReleaseRef(pFDO_DeviceData);
		return (status);
	}//~

	if (IRP_MN_SET_POWER == pIRPStack->MinorFunction) {
		fstr("\tRequest to set %s state to %s\n",
			((SystemPowerState == powerType == SystemPowerState) ?  "System" : "Device"),
			((SystemPowerState == powerType) ? 
				sysPowerState2str(powerState.SystemState) : devPowerState2str(powerState.DeviceState))
			);
	}//~

	PoStartNextPowerIrp(pIRP);

	IoSkipCurrentIrpStackLocation(pIRP);
	///status	= IoCallDriver(pFDO_DeviceData->m_pNextLowerDriver, pIRP);
	status = PoCallDriver(pFDO_DeviceData->m_pNextLowerDriver, pIRP);

	bus_ReleaseRef(pFDO_DeviceData);
	return (status);
}

NTSTATUS bus_PDO_Wakeup(t_PDO_DeviceData* pPDO_DeviceData, PIRP pIRP)
{
	PIO_STACK_LOCATION	pIRPStack		= IoGetCurrentIrpStackLocation(pIRP);
	fstr("+++ CALLED\n");
	if (NULL == pIRPStack) {
		fstr("\tNULL == pIRPStack !!! NOTHING REQUEST I/O PACKET !!!\n");
		return (STATUS_FWP_NULL_POINTER);
	}//~fi

	POWER_STATE_TYPE	powerType	= pIRPStack->Parameters.Power.Type;
	POWER_STATE			powerState	= pIRPStack->Parameters.Power.State;
	NTSTATUS            status		= STATUS_SUCCESS;
	switch (pIRPStack->MinorFunction) {
	case IRP_MN_SET_POWER:
	{
		fstr("\tSetting %s power state to %s\n",
			((SystemPowerState == powerType) ? "System" : "Device"),
			((SystemPowerState == powerType) ?
				sysPowerState2str(powerState.SystemState) : devPowerState2str(powerState.DeviceState)));

		switch (powerType) {
		case DevicePowerState:
			PoSetPowerState(pPDO_DeviceData->m_pSelfDeviceObj, powerType, powerState);
			pPDO_DeviceData->m_eDevicePowerState = powerState.DeviceState;
			status = STATUS_SUCCESS;
			break;
		case SystemPowerState:
			pPDO_DeviceData->m_eSystemPowerState = powerState.SystemState;
			status = STATUS_SUCCESS;
			break;
		default:
			status = STATUS_NOT_SUPPORTED;
			break;
		}//~htcitws
	}//~case IRP_MN_SET_POWER:
	break;

	case IRP_MN_QUERY_POWER:
		pIRP->IoStatus.Status = STATUS_SUCCESS;
		break;

	case IRP_MN_WAIT_WAKE:
	case IRP_MN_POWER_SEQUENCE:
	default:
		status = STATUS_NOT_SUPPORTED;
		break;
	}//~hctiws
	
	PoStartNextPowerIrp(pIRP);
	status = pIRP->IoStatus.Status;
	IoCompleteRequest(pIRP, IO_NO_INCREMENT);

	return (status);
}

// sa: ./network/wlan/WDI/PLATFORM/NDIS6/SDIO/N6SdioPlatformWindows.c
PCHAR powerMinorFunc2str(UCHAR minorFuncCode)
{
	switch (minorFuncCode) {
	case IRP_MN_SET_POWER:		return ("IRP_MN_SET_POWER");
	case IRP_MN_QUERY_POWER:	return ("IRP_MN_QUERY_POWER");
	case IRP_MN_POWER_SEQUENCE:	return ("IRP_MN_POWER_SEQUENCE");
	case IRP_MN_WAIT_WAKE:		return ("IRP_MN_WAIT_WAKE");
	}//~hctiws
	return ("unknown_power_irp");
}

PCHAR sysPowerState2str(__in SYSTEM_POWER_STATE eSysPowStat)
{
	switch (eSysPowStat) {
	case PowerSystemUnspecified:	return ("PowerSystemUnspecified");
	case PowerSystemWorking:		return ("PowerSystemWorking");
	case PowerSystemSleeping1:		return ("PowerSystemSleeping1");
	case PowerSystemSleeping2:		return ("PowerSystemSleeping2");
	case PowerSystemSleeping3:		return ("PowerSystemSleeping3");
	case PowerSystemHibernate:		return ("PowerSystemHibernate");
	case PowerSystemShutdown:		return ("PowerSystemShutdown");
	case PowerSystemMaximum:		return ("PowerSystemMaximum");
	}//~hctiws
	return ("UnKnown System Power State");
}

PCHAR devPowerState2str(__in DEVICE_POWER_STATE eDevPowStat)
{
	switch (eDevPowStat) {
	case PowerDeviceUnspecified:	return ("PowerDeviceUnspecified");
	case PowerDeviceD0:				return ("PowerDeviceD0");
	case PowerDeviceD1:				return ("PowerDeviceD1");
	case PowerDeviceD2:				return ("PowerDeviceD2");
	case PowerDeviceD3:				return ("PowerDeviceD3");
	case PowerDeviceMaximum:		return ("PowerDeviceMaximum");
	}//~hctiws
	return ("UnKnown Device Power State");
}

