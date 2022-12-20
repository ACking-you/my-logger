#pragma once

namespace sys
{

#define    F_OK    0    /* Check for file existence */
#define    X_OK    1    /* Check for execute permission. */
#define    W_OK    2    /* Check for write permission */
#define    R_OK    4    /* Check for read permission */

#if  defined(__WIN32__)
#include<io.h>
#include <windows.h>
	// Code for Windows platform
	using PidType = DWORD;
	using TidType = DWORD;

	inline auto GetPid() -> PidType
	{
		return GetCurrentProcessId();
	}
	inline auto GetTid() -> TidType
	{
		return GetCurrentThreadId();
	}
	inline auto GetHostname(char* name, size_t len) -> int
	{
		return gethostname(name, static_cast<int>(len));
	}
	inline auto GetLocalTime_r(time_t* timer, struct tm* tm) -> struct tm*
	{
		if (localtime_s(tm, timer) < 0)
		{
			return nullptr;
		}
		return tm;
	}
	inline auto GetStrError_r(int err_code, char* buf, size_t len) -> char*
	{
		if (strerror_s(buf, len, err_code) < 0)
		{
			return nullptr;
		}
		return buf;
	}

	inline auto CallAccess(const char* filename, int perm) -> int
	{
		return _access(filename, perm);
	}
	inline auto CallUnlockedWrite(const void* __restrict ptr, size_t size,
		size_t n, FILE* __restrict stream) -> size_t
	{
		return fwrite(ptr, size, n, stream);
	}
	inline void CallSetBuffer(FILE* stream, char* buf, unsigned size)
	{
		(void)std::setvbuf(stream, buf, _IOFBF, size);
	}

	inline auto CallFPutsUnlocked(const char* __restrict__ str, FILE* __restrict__ file) -> int
	{
		return fputs(str, file);
	}

#elif defined(__linux__)
#include <sys/types.h>
#include <sys/syscall.h>
#include <unistd.h>
	// Code for Linux platform
	using PidType = pid_t;
	using TidType = pid_t;

	inline auto GetPid() -> PidType
	{
		return getpid();
	}
	inline auto GetTid() -> TidType
	{
		return static_cast<TidType>(syscall(SYS_gettid));
	}
	inline auto GetHostname(char* name, size_t len) -> int
	{
		return gethostname(name, len);
	}
	inline auto GetLocalTime_r(time_t* timer, struct tm* tm) -> struct tm*
	{
		return localtime_r(timer, tm);
	}
	inline auto GetStrError_r(int err_code, char* buf, size_t len) -> char*
	{
		return strerror_r(err_code, buf, len);
	}
	inline auto CallAccess(const char* filename, int perm) -> int
	{
		return access(filename, perm);
	}
	inline auto CallUnlockedWrite(const void* __restrict ptr, size_t size,
		size_t n, FILE* __restrict stream) -> size_t
	{
		return fwrite_unlocked(ptr, size, n, stream);
	}
	inline void CallSetBuffer(FILE* stream, char* buf, unsigned size)
	{
		setbuffer(stream, buf, size);
	}
	inline auto CallFPutsUnlocked(const char* __restrict__ str, FILE* __restrict__ file) -> int
	{
		return fputs_unlocked(str, file);
	}
#else
#error "not supported compiler"
#endif
}