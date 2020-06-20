#pragma once

#include <vector>
#include <string>

#include <Windows.h>
#include <winnt.h>
#include <Xinput.h>

#include <SetupAPI.h>
#pragma comment(lib, "Setupapi.lib")


#if defined(_WINDLL)
#define DLL_EXPORT	__declspec(dllexport)
#else // | defined(_WINDLL)
#define DLL_EXPORT	__declspec(dllimport)
#endif//~defined(_WINDLL)



#if !defined(QWORD)
typedef DWORDLONG QWORD;
#endif//!defined(QWORD)


class DLL_EXPORT CPseudoEventTransmitterFoundation
{
public:
	CPseudoEventTransmitterFoundation() {}
	~CPseudoEventTransmitterFoundation() {}

	virtual int	initialize(const std::string key = "TEST_KEY") = 0;
	virtual int terminate() = 0;

	virtual int getDriverVersion(QWORD& qwDrvVersion) = 0;
	virtual int getBusVersion(DWORD& dwBusVersion) = 0;

	virtual int set(DWORD dwUserIdx, XINPUT_GAMEPAD& rXig) = 0;

	virtual int	getActualUserIdx(DWORD dwUserIdx) = 0;
	virtual int getQuadrantNumber(DWORD dwUserIdx) = 0;
	virtual int getVib(DWORD dwUserIdx, XINPUT_VIBRATION& rXiVib) = 0;

	virtual int plugin(const std::string& crstrIndividualCode, DWORD dwUserIdx) = 0;
	virtual int unplug(const std::string& crstrIndividualCode, DWORD dwUserIdx, bool bAll = false, bool bForce = false) = 0;

	virtual int findEmptySlot() = 0;
	virtual bool isPlugged(DWORD dwUserIdx) = 0;
	virtual bool isOwned(DWORD dwUserIdx) = 0;


protected:
	virtual int _send(void* pSend, size_t szSend, BYTE byProtoIdx, size_t& rszReceived, void* pRecv = NULL, size_t szRecv = 0) = 0;

};
///////////////////////////////////////////////
// avoid symbol mangle (__declspec(dllexport))
extern "C" __declspec(dllexport) CPseudoEventTransmitterFoundation* AllocatePseudoTransmitter();
extern "C" __declspec(dllexport) void DeallocatePseudoTransmitter(CPseudoEventTransmitterFoundation* p);
typedef CPseudoEventTransmitterFoundation*	(PFN_AllocatePseudoTransmitter)();
typedef void								(PFN_DeallocatePseudoTransmitter)(CPseudoEventTransmitterFoundation* p);


