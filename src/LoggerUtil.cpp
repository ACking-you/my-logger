//
// Created by Alone on 2022-9-21.
//

#include "LoggerUtil.h"
#include "ProcessInfo.h"

#include <cstdio>
#include<cassert>
#include <cstring>

using namespace lblog::detail;

__thread char t_errnobuf[512];
__thread char t_time[64];
__thread struct tm t_tm;
__thread time_t t_lastSecond;

const char* Util::getCurDateTime(bool isTime, time_t* now)
{
	time_t timer = time(nullptr);
	if (now != nullptr)
	{ //减少系统调用，将此时间给外界复用
		*now = timer;
	}

	if (t_lastSecond != timer)
	{
		t_lastSecond = timer;
		localtime_r(&timer, &t_tm);
	}
	int len;
	if (isTime)
	{
		len = snprintf(t_time, sizeof(t_time), "%4d-%02d-%02d %02d:%02d:%02d",
			t_tm.tm_year + 1900, t_tm.tm_mon + 1, t_tm.tm_mday,
			t_tm.tm_hour, t_tm.tm_min, t_tm.tm_sec);
		assert(len == 19);
	}
	else
	{
		len = snprintf(t_time, sizeof(t_time), "%4d-%02d-%02d",
			t_tm.tm_year + 1900, t_tm.tm_mon + 1, t_tm.tm_mday);
		assert(len == 10);
	}
	return t_time;
}

const char* Util::getErrorInfo(int error_code)
{
	return strerror_r(error_code, t_errnobuf, sizeof(t_errnobuf));
}

std::string Util::getLogFileName(const std::string& basename, time_t& now)
{
	std::string filename;

	filename.reserve(basename.size() + 64);
	filename = basename;
	char timebuf[32];
	struct tm tm{};
	localtime_r(&now, &tm);
	strftime(timebuf, sizeof timebuf, ".%Y%m%d-%H%M%S.", &tm); //除非1s刷满，否则每次得到的文件名不可能相同
	filename += timebuf;

	filename += ProcessInfo::GetHostname();

	char pidbuf[32];
	snprintf(pidbuf, sizeof pidbuf, ".%d", ProcessInfo::GetPid());
	filename += pidbuf;
	return filename;
}