// see also: https://blogs.msdn.microsoft.com/jpwdkblog/2009/07/14/setupdi-api-devcon-setupdi-api/
// sea also: https://www.codeproject.com/Articles/14412/Enumerating-windows-device

/// show symbols
// $p$g> dumpbin /EXPORTS *.dll
// $p$g> dumpbin /SYMBOLS *.dll


#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <Windows.h>
#include <winioctl.h>
#include <initguid.h>
//#include <mutex>
//#include <SetupAPI.h>
//#pragma comment(lib, "Setupapi.lib")



#include "../shared_headers/PseudoControllerBusDevice.h"
#include "../shared_headers/IoCtrl.h"

#include "./CPseudoEventTransmitterFoundation.h"


#pragma warning(disable : 4244 )


#if defined(_WINDLL)
inline void odsf(const char *fmt, ...)
{
	char		tmp[0xFFFF];
	va_list		vl;
	va_start(vl, fmt);
	vsnprintf(tmp, sizeof (tmp), fmt, vl);
#if defined(_UNICODE)
	wchar_t		wtmp[0x0002FFFF];
	//::mbstowcs(wtmp, tmp, sizeof (tmp));
	MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, tmp, -1, wtmp, sizeof (wtmp));
	OutputDebugString(wtmp);
#else// | defined(_UNICODE)
	printf(tmp);
	OutputDebugString(tmp);
#endif//~defined(_UNICODE)
	va_end(vl);
}
#if !defined(fstr)
#define fstr(fmt, ...) \
odsf("%s(%d) %s(): " fmt , __FILE__, __LINE__, __func__, __VA_ARGS__); 
#endif//~!defined(fstr)


#if !defined(ARRAY_SIZEOF)
#	define ARRAY_SIZEOF(x)			(sizeof (x) / sizeof (x[0]))
#	define array_sizeof	ARRAY_SIZEOF
#endif//~!defined(ARRAY_SIZEOF)


#if !defined(ONCE)
#define ONCE(proc)		do { static unsigned su = 0; if (0 == su) {su++; proc; } } while (0)
#endif//~!defined(ONCE)

#if !defined(TEXEC)
#define TEXEC(SEC_SPAN, PROC)	do { time_t now = ::time(NULL); static time_t stNext = 0; if (stNext <= now) { PROC; stNext	= now + SEC_SPAN; } } while (0)
#endif//~!defined(TEXEC)

#if !defined(xstrform)
inline std::string xstrform(const char* const fmt, ...)
{
	std::vector<char>	buf;
	try {
		//buf.reserve(12);		// tst
		buf.reserve(0x00020000);//K_STREAMBUF_SIZE__);
	}
	catch (std::bad_alloc& r) {
		fprintf(stderr, "___ %s, %.128s\n", r.what(), fmt);
		return ("___ CATCH THE STD::BAD_ALLOC!!!!");
	}

	va_list args, argq;
	va_start(args, fmt);
	va_copy(argq, args);	// 2nd chance. use to recover if the 1st vsnprintf() fail.

	buf.resize(buf.capacity());
	int iret = 0;
	try {
		memset(&buf[0], 0, buf.capacity());	// zero clear
		///iret = std::vsnprintf(&buf[0], buf.capacity(), fmt, args);
		iret = ::vsnprintf(&buf[0], buf.capacity(), fmt, args);
	}
	catch (std::exception& r) {
		fprintf(stderr, "___ %s\n", r.what());
		return ("___ VSNPRINTF() failure!!!  catch the exception!\n");
	}

	if (0 > iret) {
		// detected err
		fstr("head256: %.256s\n", &buf[0]);
		fstr("___ 1st vsnprintf() failure!!! iret: %d, size: %zu, capacity: %zu\n", /*r.what(),*/ iret, buf.size(), buf.capacity());
		return (&buf[0]);
	}
	else if (static_cast<size_t>(iret) >= buf.capacity()) {
		//---		bstr("+++ 1st vsnprintf() detected error!!! iret: %d, size: %zu, capacity: %zu\n", /*r.what(),*/ iret, buf.size(), buf.capacity());
		try {
			buf.reserve(buf.capacity() + iret + 1);		// +1 (NULL terminate)
		}
		catch (std::bad_alloc& r) {
			fstr("head256: %.256s\n", &buf[0]);
			fstr("___ %s. iret: %d, size: %zu, capacity: %zu\n", r.what(), iret, buf.size(), buf.capacity());
			va_end(argq);
			va_end(args);
			return (&buf[0]);
		}
		catch (std::length_error& r) {
			fstr("head256: %.256s\n", &buf[0]);
			fstr("___ %s. iret: %d, size: %zu, capacity: %zu\n", r.what(), iret, buf.size(), buf.capacity());
			va_end(argq);
			va_end(args);
			return (&buf[0]);
		}

		buf.resize(buf.capacity());
		///iret = std::vsnprintf(&buf[0], buf.capacity(), fmt, args);
		iret = ::vsnprintf(&buf[0], buf.capacity(), fmt, argq);

		if (static_cast<size_t>(iret) >= buf.capacity()) {
			fstr("head256: %.256s\n", &buf[0]);
			fstr("___ 2nd vsnprintf() failure!!! iret: %d, size: %zu, capacity: %zu\n", /*r.what(),*/ iret, buf.size(), buf.capacity());
		}
	}//~rof
	va_end(argq);
	va_end(args);
	//----	bstr("iret: %d, end sz: %zu, len: %zu, capa: %zu, %d\n", iret, buf.size(), buf.length(), buf.capacity(), buf.capacity());
	return (&buf[0]);
}
#endif//!defined(xstrform)



#if !defined(xodumpf)
///////////////////////////////////////////////////////////////////////////////
//
inline void xodumpf(const void* const psrc, size_t szLen, const char* const pForm, ...)
{
	unsigned char* pbuf = (unsigned char*)psrc;
	char szLabel[256];
	if (NULL != pForm) {
		va_list args;
		//---		::memset(szLabel, 0, sizeof (szLabel));
		va_start(args, pForm);
		::vsnprintf(szLabel, sizeof (szLabel) - 1, pForm, args);
		odsf("--- %.255s(%d):\n", szLabel, szLen);
		va_end(args);
	}

	odsf("              +0 +1 +2 +3 +4 +5 +6 +7 +8 +9 +A +B +C +D +E +F  0123456789ABCDEF\n");
	std::string strText;
	for (size_t i = 0; i <= szLen; i += 16) {
		if (0x00004000 < strText.size()) {
			odsf("%s", strText.c_str());
			strText.clear();
		}
		if (0x4000 < szLen) {
			Sleep(0);
		}

		//PTR32(strText += xstrform("  0x%08X: ", (/*pbuf +*/ i)));
		//PTR64(strText += xstrform("  0x%016llX: ", (/*pbuf +*/ i)));
		strText += xstrform("  0x%08X: ", (/*pbuf +*/ i));

		for (size_t j = 0; j < 16; j++) {
			if (i + j >= szLen) {
				strText += xstrform("   ");
			}
			else {
				strText += xstrform("%02x ", pbuf[i + j]);
			}
		}//~for (j = 0; j < 16; j++)

		strText += xstrform("[");
		for (size_t j = 0; j < 16; j++) {
			if (i + j >= szLen) {
				strText += xstrform(" ");
			}
			else {
				strText += xstrform("%c", 0x20 < pbuf[i + j] && pbuf[i + j] < 0x7f ? pbuf[i + j] : '.');
			}
		}//~for (j = 0; j < 16; j++)
		strText += xstrform("]\n");
	}//~for (i = 0; i < len ; i += 16)
	odsf("%s", strText.c_str());
	return;
}
#endif//!Xdefined(xodumpf)




class CPseudoEventTransmitterActual : public CPseudoEventTransmitterFoundation
{
public:
	CPseudoEventTransmitterActual();/// const std::string key = "ACTUAL KEY");
	~CPseudoEventTransmitterActual();

	virtual int	initialize(const std::string key = "ACTUAL KEY");
	virtual int terminate();

	virtual int getDriverVersion(QWORD& qrdwDrvVersion);
	virtual int getBusVersion(DWORD& rdwBusVersion);

	virtual int set(DWORD dwUserIdx, XINPUT_GAMEPAD& rXig);

	virtual int	getActualUserIdx(DWORD dwUserIdx);
	virtual int getQuadrantNumber(DWORD dwUserIdx);
	virtual int getVib(DWORD dwUserIdx, XINPUT_VIBRATION& rXiVib);

	virtual int plugin(const std::string& crstrIndividualCode, DWORD dwUserIdx);	// Add Device
	virtual int unplug(const std::string& crstrIndividualCode, DWORD dwUserIdx, bool bAll = false, bool bForce = false);	// Remove Deivce
	virtual int findEmptySlot();			// find empty game pad slot.
	virtual bool isPlugged(DWORD dwUserIdx);
	virtual bool isOwned(DWORD dwUserIdx);


protected:
	virtual int _send(void* pTo, size_t szSend, BYTE byProtoIdx, size_t& rszReceived, void* pRecv = NULL, size_t szRecv = 0);

private:
	HANDLE	m_hLoopback = INVALID_HANDLE_VALUE;
	XINPUT_GAMEPAD	m_xig[XUSER_MAX_COUNT];			// pad HW
	t_XIFeedback	m_xiFeedback[XUSER_MAX_COUNT];	// via APP(drv)
	std::string		m_strKey;
};


CPseudoEventTransmitterActual::CPseudoEventTransmitterActual() : m_hLoopback(INVALID_HANDLE_VALUE)
{
}

CPseudoEventTransmitterActual::~CPseudoEventTransmitterActual()
{
	if (INVALID_HANDLE_VALUE == m_hLoopback) {
		return;
	}
	terminate();
}

int	CPseudoEventTransmitterActual::initialize(const std::string key)
{
	if (std::string::npos == m_strKey.find(key)) {
		fstr("+++ updated key...\n");
		m_strKey = key;
	}
	
	if (INVALID_HANDLE_VALUE != m_hLoopback) {
		fstr("#_# already created loopback\n");
		return (0);
	}//~
	
	SP_DEVICE_INTERFACE_DATA	did;
	memset(&did, 0, sizeof (SP_DEVICE_INTERFACE_DATA));
	did.cbSize = sizeof (SP_DEVICE_INTERFACE_DATA);

	memset(m_xig, 0, sizeof (XINPUT_GAMEPAD) * XUSER_MAX_COUNT);

	HDEVINFO hPreDevInfo = ::SetupDiCreateDeviceInfoList(NULL, NULL);
	if (INVALID_HANDLE_VALUE == hPreDevInfo) {
		fstr("___ INVALID_HANDLE_VALUE == m_hLoopback\n");
		fstr("___ SetupDiCreateDeviceInfoList() failure!!! err: %d\n", ::GetLastError());
		return (-1);
	}//~

	HDEVINFO hDevInfo = ::SetupDiGetClassDevsEx(&GUID_DEVINTERFACE_PSEUDOBUS, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE, hPreDevInfo, nullptr, nullptr);
	if (NULL == hDevInfo) {
		fstr("___ SetupDiGetClassDevsEx() failure!!!\n");
		return (-2);
	}

	DWORD dwMemberIdx = 0;
	while (::SetupDiEnumDeviceInterfaces(hDevInfo, nullptr, &GUID_DEVINTERFACE_PSEUDOBUS, dwMemberIdx, &did)) {
		DWORD dwRequiredSize = 0;
		::SetupDiGetDeviceInterfaceDetail(hDevInfo, &did, nullptr, 0, &dwRequiredSize, nullptr);

		// allocate target buffer
		std::vector<char>	vaBuf(dwRequiredSize);	// reserve
		SP_DEVICE_INTERFACE_DETAIL_DATA* detailDataBuffer = (SP_DEVICE_INTERFACE_DETAIL_DATA*)vaBuf.data();
		///SP_DEVICE_INTERFACE_DETAIL_DATA* detailDataBuffer = (SP_DEVICE_INTERFACE_DETAIL_DATA*)new char[dwRequiredSize];
		detailDataBuffer->cbSize = sizeof (SP_DEVICE_INTERFACE_DETAIL_DATA);

		// get detail buffer
		if (!::SetupDiGetDeviceInterfaceDetail(hDevInfo, &did, detailDataBuffer, dwRequiredSize, &dwRequiredSize, nullptr)) {
			fstr("___ SetupDiGetDeviceInterfaceDetail() failure!!! gle: %d\n", ::GetLastError());
			::SetupDiDestroyDeviceInfoList(hDevInfo);
			vaBuf.clear();///delete[](detailDataBuffer);
			continue;
		}//~

///#define K_LOOPBACK_DEVICE_NAME ("\\\\.\\LBKFLT_PSEUDODEVICEBUS")
		// DevicePath: \\?\root#system#0001#{b8c11890-cac8-4820-bb2a-c49c483dece5}
		// detailDataBuffer->DevicePath: \\?\root\#system#000N#{GUID_DEVINTERFACE_PSEUDOBUS}	// ref: PseudoControllerBusDevice.h
		fstr("+++ [%4d] DevicePath: %s\n", dwMemberIdx, detailDataBuffer->DevicePath);
		m_hLoopback = ::CreateFile(detailDataBuffer->DevicePath,	// K_LOOPBACK_DEVICE_NAME
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			nullptr,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
			nullptr);
		fstr("+++ m_hLoopback: 0x%p\n", m_hLoopback);
		vaBuf.clear();///delete[](detailDataBuffer);
		dwMemberIdx++;
	}//~elihw

	return (0);
}
int CPseudoEventTransmitterActual::terminate()
{
	if (INVALID_HANDLE_VALUE == m_hLoopback) {
		return (1);
	}
	
	::CloseHandle(m_hLoopback);
	m_hLoopback	= INVALID_HANDLE_VALUE;
	return (0);
}

int CPseudoEventTransmitterActual::_send(void* pSend, size_t szSend, BYTE byProtoIdx, size_t& rszReceived, void* pRecv/*=NULL*/, size_t szRecv/*=0*/)
{
	if (INVALID_HANDLE_VALUE == m_hLoopback) {
		fstr("___ INVALID_HANDLE_VALUE == m_hLoopback\n");
		return (-1);
	}//~fi
	if (byProtoIdx < E_PROTO_NONE || E_PROTO_MAX < byProtoIdx) {
		fstr("___ invalid protocol idx. idx: 0x%02X\n", byProtoIdx);
		return (-4);
	}
	t_ProtocolHdr ph;
	ph.byProtocolIdx	= byProtoIdx;
	ph.byChunkSize		= (BYTE)szSend;
	std::vector<BYTE> vaSend(sizeof (t_ProtocolHdr) + szSend);// = { 0 };
	memcpy(&vaSend[0], &ph, sizeof (t_ProtocolHdr));
	if (0 < szSend) {
		memcpy(&vaSend[sizeof (t_ProtocolHdr)], pSend, szSend);
	}//~fi
	///xodumpf(vaSend.data(), vaSend.size(), "%u", szSend);

	DWORD dwReceived = 0;
	BOOL bRet = ::DeviceIoControl(m_hLoopback, IOCTL_BUS_PROTOCOL, static_cast<LPVOID>(vaSend.data()), static_cast<DWORD>(vaSend.size()), pRecv, static_cast<DWORD>(szRecv), &dwReceived, nullptr);
	if (!bRet) {
		DWORD dwErr = ::GetLastError();	// GLE: ERROR_INVALID_PARAMETER?
		TEXEC(2, fstr("___ DeviceIoControl(IOCTL_BUS_PROTOCOL: protoIdx: %s(0x%02X), failure!!! GLE: 0x%08X(%d)\n", protoid2human(byProtoIdx), byProtoIdx, dwErr, dwErr));
		return (-10);
	}//~fi
	rszReceived = dwReceived;

	if (szRecv != dwReceived) {
		// probably not. drv/dll iface DESIGN ERROR. chk ur busenum.c IOCTL_ProtocolParse(),
		///ONCE(fstr("#_# SYSTEM DESIGN ERROR!!! Insufficient receive buffer size for received size. protoIdx: 0x%02X, required: 0x%08X, received: 0x%08X\n", byProtoIdx, szRecv, dwReceived));
		TEXEC(7, fstr("#_# SYSTEM DESIGN ERROR!!! Insufficient receive buffer size for received size. protoIdx: 0x%02X, required: 0x%08X, received: 0x%08X\n", byProtoIdx, szRecv, dwReceived));
		///return (static_cast<int>(vaSend.size() - dwReceived));
		///return (-20);
	}//~
	
	return (0);
}




int CPseudoEventTransmitterActual::getDriverVersion(QWORD& rqwDrvVersion)
{
	HDEVINFO hDevInfo = ::SetupDiGetClassDevs(&GUID_DEVINTERFACE_PSEUDOBUS, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
	for (int i = 0; ; i++) {
		SP_DEVINFO_DATA	did;
		memset(&did, 0, sizeof (SP_DEVINFO_DATA));
		did.cbSize = sizeof (SP_DEVINFO_DATA);
		if (!::SetupDiEnumDeviceInfo(hDevInfo, i, &did)) {
			break;
		}

		SP_DEVINSTALL_PARAMS	dip;
		memset(&dip, 0, sizeof (SP_DEVINSTALL_PARAMS));
		dip.cbSize = sizeof (SP_DEVINSTALL_PARAMS);
		if (!::SetupDiGetDeviceInstallParams(hDevInfo, &did, &dip)) {
			fstr("___ SetupDiGetDeviceInstallParams() failure!!! gle: %d\n", ::GetLastError());
			return (-10);
		}
		else {
			dip.FlagsEx |= DI_FLAGSEX_INSTALLEDDRIVER;
			if (!::SetupDiSetDeviceInstallParams(hDevInfo, &did, &dip)) {
				fstr("___ SetupDiGetDeviceInstallParams() failure!!! gle: %d\n", ::GetLastError());
				return (-11);
			}
		}

		// Build a list of driver info items that we will retrieve below
		if (!::SetupDiBuildDriverInfoList(hDevInfo, &did, SPDIT_COMPATDRIVER)) {
			fstr("___ SetupDiBuildDriverInfoList() failure!!! gle: %d\n", ::GetLastError());
			return (-21);
		}

		// sa: https://stackoverflow.com/questions/16032789/how-do-i-get-the-version-of-a-driver-on-windows-from-c
		// sa: https://stackoverflow.com/questions/16063402/why-does-setupdienumdriverinfo-give-two-version-numbers-for-my-driver
		// Get all the info items for this driver
		for (int j = 0; ; j++) {
			SP_DRVINFO_DATA drvInfoData;
			memset(&drvInfoData, 0, sizeof (SP_DRVINFO_DATA));
			drvInfoData.cbSize = sizeof (SP_DRVINFO_DATA);
			if (!::SetupDiEnumDriverInfo(hDevInfo, &did, SPDIT_COMPATDRIVER, j, &drvInfoData)) {
				break;
			}//~

			std::string strDrvType;
			switch (drvInfoData.DriverType) {
			case SPDIT_CLASSDRIVER:		strDrvType = "SPDIT_CLASSDRIVER";	break;
			case SPDIT_COMPATDRIVER:	strDrvType = "SPDIT_COMPATDRIVER";	break;
			}//~hctiws

			// DriverVersion type is QWORD
			fstr("+++ [%d] Type:[%s], Description:[%s], ProviderName:[%s], DriverVersion:[0x%016llX(%llu)]\n", j, strDrvType.c_str(), drvInfoData.Description, drvInfoData.ProviderName, drvInfoData.DriverVersion, drvInfoData.DriverVersion);
			fstr("+++ DriverVersion: [0x%08X, 0x%08X]\n", drvInfoData.DriverVersion >> 32, drvInfoData.DriverVersion & 0xFFFFFFFF);
			
			unsigned uHour = (unsigned)((drvInfoData.DriverVersion >> 48));
			unsigned uMin = (unsigned)((drvInfoData.DriverVersion >> 32)	& 0xFFFF);
			unsigned uSec = (unsigned)((drvInfoData.DriverVersion >> 16)	& 0xFFFF);
			unsigned uMs = (unsigned)((drvInfoData.DriverVersion			& 0xFFFF));
			fstr("+++ VersionTime: [%u.%u.%u.%u]\n", uHour, uMin, uSec, uMs);

			rqwDrvVersion = drvInfoData.DriverVersion;
			xodumpf(&drvInfoData.DriverVersion, sizeof (QWORD), "DrvVersion");
		}//~rof
	}//~rof

	return (0);
}
int CPseudoEventTransmitterActual::getBusVersion(DWORD& rdwBusVersion)
{
	if (INVALID_HANDLE_VALUE == m_hLoopback) {
		fstr("___ INVALID_HANDLE_VALUE == m_hLoopback\n");
		return (-1);
	}

	t_BusEnumVersionRes res;
	size_t szReceived = 0;
	int iret	= _send(NULL, 0, E_PROTO_VER, szReceived, &res, sizeof (t_BusEnumVersionRes));
	if (0 > iret) {
		fstr("___ _send(, , E_PROTO_VER) failure!!! ret: %d\n", iret);
		return (iret);
	}//~

	xodumpf(&res, sizeof (t_BusEnumVersionRes), "BusVersion");
	if (sizeof(t_BusEnumVersionRes) == szReceived) {
		rdwBusVersion = res.dwVersion;
	}//~fi
	return (0);
}

int	CPseudoEventTransmitterActual::set(DWORD dwUserIdx, XINPUT_GAMEPAD& rXig)
{
	memcpy_s(&m_xig[dwUserIdx], sizeof (XINPUT_GAMEPAD), &rXig, sizeof (XINPUT_GAMEPAD));
	if (INVALID_HANDLE_VALUE == m_hLoopback) {
		fstr("___ INVALID_HANDLE_VALUE == m_hLoopback\n");
		return (-10);
	}//~
	if ((dwUserIdx < 0) || (3 < dwUserIdx)) {
		fstr("___ INDALID user idx. dwUserIdx: %d\n", dwUserIdx);
		return (-20);
	}//~
	

	t_XIFeedback rep;
	t_BusEnumReportReq req;
	req.m_bySize	= sizeof (t_BusEnumReportReq);
	req.m_byIdx		= dwUserIdx + 1;		// 1 origin
	req.m_byData[1] = sizeof (t_XinputGamePadClone);
	req.m_byData[0] = 0x00;
	memcpy_s(&req.m_byData[2], sizeof (XINPUT_GAMEPAD), &m_xig[dwUserIdx], sizeof (XINPUT_GAMEPAD));

	size_t szReceived = 0;
	int iret = _send(&req, sizeof (t_BusEnumReportReq), E_PROTO_REPORT, szReceived, &rep, sizeof (t_XIFeedback));
	if (0 > iret) {
		fstr("___ _send() failure!! ret: %d\n", iret);
		return (iret);
	}//~

	if (sizeof (t_XIFeedback) == szReceived) {
		///TEXEC(1, fstr("RINGLIGHT: %u, %u\n", dwUserIdx, rep.wRingLight));
		memcpy_s(&m_xiFeedback[dwUserIdx], sizeof (t_XIFeedback), &rep, sizeof (t_XIFeedback));
	}//~fi
	///TEXEC(1, xodumpf(&m_xiFeedback, sizeof (t_XIFeedback) * XUSER_MAX_COUNT, "idx: %u", dwUserIdx));
	
	return (0);
}

int CPseudoEventTransmitterActual::getActualUserIdx(DWORD dwUserIdx)
{
	if (INVALID_HANDLE_VALUE == m_hLoopback) {
		fstr("___ INVALID_HANDLE_VALUE == m_hLoopback\n");
		return (-1);
	}
	int iret = set(dwUserIdx, m_xig[dwUserIdx]);
	if (0 > iret) {
		return (iret);
	}
	return (m_xiFeedback[dwUserIdx].wRingLight);
}
int CPseudoEventTransmitterActual::getQuadrantNumber(DWORD dwUserIdx)
{
	if (INVALID_HANDLE_VALUE == m_hLoopback) {
		fstr("___ INVALID_HANDLE_VALUE == m_hLoopback\n");
		return (-1);
	}
	int iret = set(dwUserIdx, m_xig[dwUserIdx]);
	if (0 > iret) {
		return (iret);
	}
	return (m_xiFeedback[dwUserIdx].wRingLight);
}
int CPseudoEventTransmitterActual::getVib(DWORD dwUserIdx, XINPUT_VIBRATION& rXiVib)
{
	if (INVALID_HANDLE_VALUE == m_hLoopback) {
		fstr("___ INVALID_HANDLE_VALUE == m_hLoopback\n");
		return (-1);
	}
	int iret = set(dwUserIdx, m_xig[dwUserIdx]);
	if (0 > iret) {
		return (iret);
	}
	rXiVib.wLeftMotorSpeed = m_xiFeedback[dwUserIdx].wLeftMotorSpeed;
	rXiVib.wRightMotorSpeed = m_xiFeedback[dwUserIdx].wRightMotorSpeed;
	return (0);
}

int CPseudoEventTransmitterActual::plugin(const std::string& crstrIndividualCode, DWORD dwUserIdx)	// Add Device
{
	if (INVALID_HANDLE_VALUE == m_hLoopback) {
		fstr("___ INVALID_HANDLE_VALUE == m_hLoopback\n");
		return (-1);
	}
	if ((dwUserIdx < 0) || (3 < dwUserIdx)) {
		fstr("___ INDALID user idx. dwUserIdx: %d\n", dwUserIdx);
		return (-2);
	}
	if (5 < crstrIndividualCode.size()) {
		ONCE(fstr("#_# invalid individual code size... %u/%u\n", crstrIndividualCode.size(), 5));
	}//~

	t_BusEnumPluginReq req = { 0 };
	req.byIdx = (BYTE)dwUserIdx + 1;		// 1 origin
	memcpy_s(&req.byIndividualCode, array_sizeof (req.byIndividualCode), crstrIndividualCode.data(), array_sizeof (req.byIndividualCode));
	memcpy_s(&req.byaKey, array_sizeof (req.byaKey), m_strKey.c_str(), m_strKey.size());
	size_t szReceived  = 0;
	int iret	= _send(&req, sizeof (t_BusEnumPluginReq), E_PROTO_PLUGIN, szReceived);
	if (0 > iret) {
		fstr("___ _send(, , E_PROTO_PLUGIN) failure!!! ret: %d\n", iret);
		return (iret);
	}

	return (0);
}

int CPseudoEventTransmitterActual::unplug(const std::string& crstrIndividualCode, DWORD dwUserIdx, bool bAll, bool bForce)
{
	if (INVALID_HANDLE_VALUE == m_hLoopback) {
		fstr("___ INVALID_HANDLE_VALUE == m_hLoopback\n");
		return (-1);
	}//~

	t_BusEnumUnplugReq req = {0};
	req.byIdx = (BYTE)dwUserIdx + 1;	// 1 origin
	memcpy_s(&req.byIndividualCode, array_sizeof (req.byIndividualCode), crstrIndividualCode.data(), array_sizeof (req.byIndividualCode));
	if (bAll) {
		req.byAll = 1;
	}//~fi
	if (bForce) {
		req.byForce = 1;
	}//~fi
	size_t szReceived = 0;
	int iret	= _send(&req, sizeof (t_BusEnumUnplugReq), E_PROTO_UNPLUG, szReceived);
	if (0 > iret) {
		fstr("___ _send(, , E_PROTO_UNPLUG) failure!!! ret: %d\n", iret);
		return (iret);
	}
	return (0);
}

int CPseudoEventTransmitterActual::findEmptySlot()			// find empty game pad slot.
{
	if (INVALID_HANDLE_VALUE == m_hLoopback) {
		fstr("___ INVALID_HANDLE_VALUE == m_hLoopback\n");
		return (-1);
	}
	t_BusEnumFindEmptySlotRes res;
	size_t szReceived = 0;
	int iret	= _send(NULL, 0, E_PROTO_FIND_EMPTY_SLOT, szReceived, &res, sizeof (t_BusEnumFindEmptySlotRes));
	if (0 > iret) {
		fstr("___ _send(, , E_PROTO_FIND_EMPTY_SLOT) failure!!! ret: %d\n", iret);
		return (iret);
	}
	
	return (res.byEmptySlot);
}
bool CPseudoEventTransmitterActual::isPlugged(DWORD dwUserIdx)
{
	if (INVALID_HANDLE_VALUE == m_hLoopback) {
		fstr("___ INVALID_HANDLE_VALUE == m_hLoopback\n");
		return (false);
	}

	if ((dwUserIdx < 0) || (3 < dwUserIdx)) {
		fstr("___ INDALID user idx. dwUserIdx: %d\n", dwUserIdx);
		return (false);
	}
	t_BusEnumIsPlugedReq	req;
	t_BusEnumIsPlugedRep	rep;
	req.byIdx = (BYTE)dwUserIdx + 1;
	size_t szReceived = 0;
	int iret	= _send(&req, sizeof (t_BusEnumIsPlugedReq), E_PROTO_IS_PLUGED, szReceived, &rep, sizeof (t_BusEnumIsPlugedRep));
	if (0 > iret) {
		fstr("___ _send(, , E_PROTO_IS_PLUGED) failure!!! ret: %d\n", iret);
		return (iret);
	}

	///fstr("+++ [idx: %u] stat: %d\n", dwUserIdx, rep.byState);
	if (szReceived == sizeof (t_BusEnumIsPlugedRep) && 0 == rep.byState) {
		return (false);
	}
	return (true);
}

bool CPseudoEventTransmitterActual::isOwned(DWORD dwUserIdx)
{
	if (INVALID_HANDLE_VALUE == m_hLoopback) {
		fstr("___ INVALID_HANDLE_VALUE == m_hLoopback\n");
		return (false);
	}
	if ((dwUserIdx < 0) || (3 < dwUserIdx)) {
		fstr("___ INDALID user idx. dwUserIdx: %d\n", dwUserIdx);
		return (false);
	}
	
	t_BusEnumProcIDRep rep;
	t_BusEnumProcIDReq req;
	req.byIdx = (BYTE)(dwUserIdx + 1);
	size_t szReceived = 0;
	int iret	= _send(&req, sizeof (t_BusEnumProcIDReq), E_PROTO_PROC_ID, szReceived, &rep, sizeof (t_BusEnumProcIDRep));
	if (0 > iret) {
		fstr("___ _send(, , E_PROTO_PROC_ID) failure!!! ret: %d\n", iret);
		return (false);
	}

	if (sizeof(t_BusEnumProcIDRep) != szReceived) {
		return (false);
	}

	fstr("+++ app pid: %u : drv registered pid: %u\n", ::GetCurrentProcessId(), rep.dwPID);
	if (::GetCurrentProcessId() != rep.dwPID) {
		return (false);
	}
	return (true);
}


#endif//~defined(_WINDLL)


__declspec(dllexport) CPseudoEventTransmitterFoundation* AllocatePseudoTransmitter()
{
	CPseudoEventTransmitterActual* pPETA = NULL;
	try {
		pPETA = new CPseudoEventTransmitterActual();
	}
	catch (std::bad_alloc& r) {
		fstr("__ %s\n", r.what());
		return (NULL);
	}//~
	return (pPETA);
}
__declspec(dllexport) void DeallocatePseudoTransmitter(CPseudoEventTransmitterFoundation* p)
{
	if (NULL == p) {
		return;
	}
	delete p;
	return;
}


BOOL __stdcall DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(hModule);
	UNREFERENCED_PARAMETER(lpReserved);

	switch (dwReason) {
	case DLL_PROCESS_ATTACH:
		break;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		break;
	}//~hctiws
	return (TRUE);
}

