#pragma once

#ifdef _KERNEL_MODE
	#include <ntddk.h>
	#define NTSTRSAFE_LIB
	#include <ntstrsafe.h>
#else // | defined(_KERNEL_MODE)
	#include <stdio.h>
	#include <time.h>
	#include <windows.h>
#endif//~defined(_KERNEL_MODE)

#include <evntrace.h>



///
#define K_MODE_DEVEL	(0)	// 0 on RELEASE, 1 on DEVEL
///#define K_MODE_DEVEL	(1)	// 0 on RELEASE, 1 on DEVEL



////////////////////////////////////////////////////////////////////
//

#if !defined(STRINGIFY)
#	define STRINGIFY(x) #x
#	define TOSTRING(x) STRINGIFY(x)
#	define AT__ "{" __FILE__ "}, " TOSTRING(__LINE__) ", "
#endif//~!defined(STRINGIFY)


#if 0 == K_MODE_DEVEL
#	define fstr(fmt, ...)	do { } while (0);
#else // K_MODE_DEVEL
# if defined(K_PROJECT_NAME)
#	define fstr(fmt, ...) cstr(DPFLTR_IHVDRIVER_ID, DPFLTR_TRACE_LEVEL, "%s: %s(%d) %s(): " fmt , K_PROJECT_NAME, __FILE__, __LINE__, __func__, __VA_ARGS__); 
# else// | defined(K_PROJECT_NAME)
#	define fstr(fmt, ...) cstr(DPFLTR_IHVDRIVER_ID, DPFLTR_TRACE_LEVEL, "%s(%d) %s(): " fmt , __FILE__, __LINE__, __func__, __VA_ARGS__); 
# endif//~defined(K_PROJECT_NAME)
#endif~//1 == K_MODE_DEVEL

#undef TOSTRING
#undef STRINGIFY


#define ARRAY_SIZEOF(x)			(sizeof (x) / sizeof (x[0]))
#define array_sizeof	ARRAY_SIZEOF

#define GUID_FORMAT "{%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX}"
#define GUID_ARG(guid) guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3], guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]
#define GUID_ARGP(pGuid) pGuid->Data1, pGuid->Data2, pGuid->Data3, pGuid->Data4[0], pGuid->Data4[1], pGuid->Data4[2], pGuid->Data4[3], pGuid->Data4[4], pGuid->Data4[5], pGuid->Data4[6], pGuid->Data4[7]


// see also: https://docs.microsoft.com/ja-jp/windows-hardware/drivers/wdf/how-to-generate-a-umdf-driver-from-a-kmdf-driver
#if defined(_KERNEL_MODE)
# define KMODE(proc)	proc
# define UMODE(proc)
#else// | defined(_KERNEL_MODE)
# define KMODE(proc)
# define UMODE(proc) proc
#endif//~defined(_KERNEL_MODE)


#if defined(_KERNEL_MODE)
// see also: https://github.com/Microsoft/Windows-driver-samples/blob/master/avstream/avscamera/sys/Dbg.h
// see also: https://community.osr.com/discussion/259310
inline time_t UTCSeconds(time_t* p) {
	UNREFERENCED_PARAMETER(p);
	LARGE_INTEGER st;
	TIME_FIELDS tf;
	KeQuerySystemTime(&st);
	RtlTimeToTimeFields(&st, &tf);

	time_t now	= tf.Second
		+ 60 * tf.Minute
		+ 60 * tf.Hour
		+ 24 * tf.Month
		+ 24 * tf.Day
		+ 365 * (tf.Year);
	return (now);
}
#else // | defined(_KERNEL_MODE)
#define UTCSeconds	time
#endif//~defined(_KERNEL_MODE)

///////////////////////////////////////////////////////////////////////////////
//@DESC
#define ONCE(proc)		do { static unsigned su = 0; if (0 == su) {su++; proc; } } while (0)
#define NEXEC(N, proc)	do { static unsigned su = 0; if (N >  su) {su++; proc; } } while (0)
///
#define TEXEC(SEC_SPAN, PROC)	do { time_t now = UTCSeconds/*time*/(NULL); static time_t stNext = 0; if (stNext <= now) { PROC; stNext	= now + SEC_SPAN; } } while (0)


// see also ntsd command lists: ftp://supermicro.nl/utility/H9.5/hct/docs/ntdebug.htm

// NTSTATUS error list
// see also: http://accelart.jp/blog/NTSTATUSErrMsgJa.html
// see also: https://community.osr.com/discussion/4814/dbgprint-and-unicode-strings
// see also: https://blogs.msdn.microsoft.com/doronh/2006/03/03/string-buffers-and-irql/
///#define NTSTRSAFE_LIB
///#include <ntstrsafe.h>

#define align__(size__,aligned__) (size__?(((size__+(aligned__-1))/aligned__)*aligned__):aligned__)


#if 0
https://community.osr.com/discussion/4814/dbgprint-and-unicode-strings
1. %s (PCHAR)
2. %ws (PWCHAR)
3. %Z (PSTRING, PANSI_STRING, POEM_STRING)
4. %wZ (PUNICODE_STRING)
#endif//~if0



///////////////////////////////////////////////////////////////////////////////
//
#if defined(_KERNEL_MODE)
#define cstr	DbgPrintEx
#else // | defined(_KERNEL_MODE)
inline void cstr(int a, int b, const char* const fmt, ...)
{
	NTSTATUS status = 0;
	char buf[0x2800];
	va_list args;
	va_start(args, fmt);
	UMODE(status = vsnprintf          (buf, sizeof (buf)-2, fmt, args));
	va_end(args);
	if (NT_SUCCESS(status)) {
		KMODE(DbgPrintEx(a,b, buf));
		UMODE(UNREFERENCED_PARAMETER(a));
		UMODE(UNREFERENCED_PARAMETER(b));
		UMODE(OutputDebugStringA(buf));
	}
}
#endif//~defined(_KERNEL_MODE)


///////////////////////////////////////////////////////////////////////////////
//
#if defined(_KERNEL_MODE)
inline int DrvSnprintf(char* target, size_t len, const char* const fmt, ...)
{
	NTSTATUS status;
	va_list args;
	va_start(args, fmt);
	status	= RtlStringCchPrintfA(target, len, fmt, args);
	va_end(args);
	if (NT_SUCCESS(status)) {
	}//~
	return (status);
}
#else // | defined(_KERNEL_MODE)
#define DrvSnprintf snprintf		// UMODE
#endif//~defined(_KERNEL_MODE)


///////////////////////////////////////////////////////////////////////////////
//
#if defined(_KERNEL_MODE)
inline int DrvVsnprintf(char* target, size_t len, const char* const fmt, ...)
{
	NTSTATUS status;
	char szBuf[8192];
	va_list args;
	va_start(args, fmt);
	status = RtlStringCbVPrintfA(szBuf, sizeof (szBuf)-2, fmt, args);
	va_end(args);
	if (NT_SUCCESS(status)) {
		strncpy(target, szBuf, len);
	}
	return (status);
}
#else // | defined(_KERNEL_MODE)
#define DrvVsnprintf vsnprintf		// UMODE
#endif//~defined(_KERNEL_MODE)

///////////////////////////////////////////////////////////////////////////////
//
inline void strncatf(char* target, size_t len, const char* const fmt, ...)
{
	NTSTATUS status;
	char szBuf[8192];
	va_list args;
	va_start(args, fmt);
	KMODE(status = RtlStringCbVPrintfA(szBuf, sizeof (szBuf)-2, fmt, args));
	UMODE(status = vsnprintf          (szBuf, sizeof (szBuf)-2, fmt, args));
	va_end(args);
	if (NT_SUCCESS(status)) {
		strncat_s(target, len - 2, szBuf, strlen(szBuf));
	}//~fi
}


///////////////////////////////////////////////////////////////////////////////
//
inline NTSTATUS KernSleep(unsigned ms)
{
    LARGE_INTEGER interval;
    NTSTATUS status;
	interval.QuadPart = ms * 1000;
    status = KeDelayExecutionThread(KernelMode, FALSE, &interval);
    return (status);
}

///////////////////////////////////////////////////////////////////////////////
//
inline void odumpf(const void* const psrc, size_t szLen, const char* const pForm, ...)
{
	unsigned char* pbuf = (unsigned char*)psrc;
	char szLabel[0xFF];
	if (NULL != pForm) {
		va_list args;
		va_start(args, pForm);
		NTSTATUS status;
		status = DrvVsnprintf(szLabel, sizeof (szLabel) - 2, pForm, args);
		va_end(args);
		if (NT_SUCCESS(status)) {
			cstr(DPFLTR_IHVDRIVER_ID, DPFLTR_TRACE_LEVEL, "--- %.255s(%d):\n", szLabel, szLen);
		}//~
	}//~
	cstr(DPFLTR_IHVDRIVER_ID, DPFLTR_TRACE_LEVEL, "              +0 +1 +2 +3 +4 +5 +6 +7 +8 +9 +A +B +C +D +E +F  0123456789ABCDEF\n");

	char szBuf[0x2800]	= {0};
	for (size_t i = 0; i <= szLen; i += 16) {
		///if (0x2000 < strlen(szBuf))
		if (400 < strlen(szBuf))	// ref: KdPrintEx() restriction
		{
			cstr(DPFLTR_IHVDRIVER_ID, DPFLTR_TRACE_LEVEL, "%s", szBuf);
			szBuf[0]	= '\0';
			///KernSleep(2);
		}
		strncatf(szBuf, sizeof (szBuf), "  0x%08X: ", /*pbuf +*/(unsigned int)i);

		for (size_t j = 0; j < 16; j++) {
			if (i + j >= szLen) {
				strncatf(szBuf, sizeof (szBuf), "   ");
			}//~fi
			else {
				strncatf(szBuf, sizeof (szBuf), "%02x ", pbuf[i + j]);
			}//~esle
		}//~for (j = 0; j < 16; j++)

		strncatf(szBuf, sizeof (szBuf), "[");
		for (size_t j = 0; j < 16; j++) {
			if (i + j >= szLen) {
				strncatf(szBuf, sizeof (szBuf), " ");
			}//~
			else {
				char ch = 0x20 < pbuf[i + j] && pbuf[i + j] < 0x7f ? pbuf[i + j] : '.';
				strncatf(szBuf, sizeof (szBuf), "%c", ch);
			}//~esle
		}//~for (j = 0; j < 16; j++)
		strncatf(szBuf, sizeof(szBuf), "]\n");
	}//~for (i = 0; i < len ; i += 16)

	cstr(DPFLTR_IHVDRIVER_ID, DPFLTR_TRACE_LEVEL, "%s", szBuf);
	return;
}


//////////////////////////////////////////////////////////
//
#if defined(_KERNEL_MODE)
#pragma warning(disable: 4995)
inline VOID LogEvent(NTSTATUS code, PWSTR msg, PVOID pObj)
{
	PIO_ERROR_LOG_PACKET p;
	USHORT DumpDataSize = 0;
	ULONG packetlen = sizeof (IO_ERROR_LOG_PACKET) + DumpDataSize;
	if (NULL != msg) {
		packetlen += (ULONG)((wcslen(msg) + 1) * sizeof(WCHAR));
	}

	if (ERROR_LOG_MAXIMUM_SIZE < packetlen) {
		return;
	}
	p = (PIO_ERROR_LOG_PACKET)IoAllocateErrorLogEntry(pObj, (UCHAR)packetlen);
	if (NULL == p) {
		return;
	}

	memset(p, 0, sizeof (IO_ERROR_LOG_PACKET));
	p->ErrorCode = code;
	p->FinalStatus = 0;

	p->DumpDataSize = DumpDataSize;
	//p->DumpData[0] = <whatever>;

	p->StringOffset = sizeof (IO_ERROR_LOG_PACKET) + p->DumpDataSize;

	if (NULL != msg) {
		p->NumberOfStrings = 1;
		// wcscpy((PWSTR)((PUCHAR)p + p->StringOffset), msg);
		wcscpy_s((PWSTR)((PUCHAR)p + p->StringOffset), (ULONG)((wcslen(msg) + 1)), msg);
	}
	else {
		p->NumberOfStrings = 0;
	}//~

	IoWriteErrorLogEntry(p);
}

inline VOID LogEventWithStatus(NTSTATUS code, PWSTR msg, PVOID pObj, NTSTATUS stat)
{
	WCHAR  strStat[12];
	PIO_ERROR_LOG_PACKET p;
	USHORT DumpDataSize = 0;
	ULONG packetlen;

	RtlStringCchPrintfW((NTSTRSAFE_PWSTR)strStat, 12, L"0x%08x", stat);

	packetlen = (ULONG)(sizeof (IO_ERROR_LOG_PACKET) + DumpDataSize + (wcslen(strStat) + 1) * sizeof (WCHAR));
	if (NULL != msg) {
		packetlen += (ULONG)((wcslen(msg) + 1) * sizeof (WCHAR));
	}

	if (ERROR_LOG_MAXIMUM_SIZE < packetlen) {
		return;
	}
	p = (PIO_ERROR_LOG_PACKET)IoAllocateErrorLogEntry(pObj, (UCHAR)packetlen);
	if (NULL == p) {
		return;
	}

	memset(p, 0, sizeof (IO_ERROR_LOG_PACKET));
	p->ErrorCode = code;
	p->FinalStatus = stat;
	p->DumpDataSize = DumpDataSize;
	//p->DumpData[0] = <whatever>;
	p->StringOffset = sizeof (IO_ERROR_LOG_PACKET) + p->DumpDataSize;

	if (NULL != msg) {
		p->NumberOfStrings = 2;
		wcscpy_s((PWSTR)((PUCHAR)p + p->StringOffset), (ULONG)(wcslen(msg) + 1), msg);
		wcscpy_s((PWSTR)((PUCHAR)p + p->StringOffset + (wcslen(msg) + 1) * sizeof (WCHAR)), wcslen(strStat) + 1, strStat);
	}
	else {
		p->NumberOfStrings = 1;
		wcscpy_s((PWSTR)((PUCHAR)p + p->StringOffset), wcslen(strStat) + 1, strStat);
	}

	IoWriteErrorLogEntry(p);
}
#else// | defined(_KERNEL_MODE)

///LogEventWithStatus(NTSTATUS code, PWSTR msg, PVOID pObj, NTSTATUS stat)

//#define LogEventWithStatus(code, msg, pObj, stat, fmt, ...) \
//	cstr(DPFLTR_IHVDRIVER_ID, DPFLTR_TRACE_LEVEL, "%s(%d) %s(): " "code:0x%08X, msg:%wZ, stat:0x%08X" fmt , __FILE__, __LINE__, __func__, code, msg, stat, __VA_ARGS__); 

inline VOID LogEventWithStatus(NTSTATUS code, PWSTR msg, LPVOID pObj, NTSTATUS stat)
{
	UNREFERENCED_PARAMETER(pObj);
	char buf[256];
	snprintf(buf, sizeof (buf), "code:0x%08x, msg:%wZ, stat:0x%08X\n", code, msg, stat);
	DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_TRACE_LEVEL, buf);
	return;
}
#endif//~defined(_KERNEL_MODE)



