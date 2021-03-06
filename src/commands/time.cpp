#include <cstdint>
#include <cstdlib>
#include "logger/logger.h"
#include "mission.h"
#include "obc_access.hpp"
#include "terminal/terminal.h"
#include "time/TimePoint.h"
#include "time/timer.h"

using std::chrono::seconds;
using std::chrono::milliseconds;
using std::chrono::duration_cast;

void JumpToTimeHandler(uint16_t argc, char* argv[])
{
    if (argc != 1)
    {
        GetTerminal().Puts("jumpToTime <time>\n");
        return;
    }

    char* tail;
    const auto targetTime = milliseconds(strtoul(argv[0], &tail, 10));
    devices::rtc::RTCTime rtcTime;
    if (OS_RESULT_FAILED(GetRTC().ReadTime(rtcTime)))
    {
        LOG(LOG_LEVEL_ERROR, "Unable to retrieve time from external RTC");
        return;
    }

    const auto externalTime = duration_cast<milliseconds>(rtcTime.ToDuration());

    LOGF(LOG_LEVEL_INFO,
        "Jumping to time 0x%x%xms 0x%x%xms\n",
        static_cast<unsigned int>(targetTime.count() >> 32),
        static_cast<unsigned int>(targetTime.count()),
        static_cast<unsigned int>(externalTime.count() >> 32),
        static_cast<unsigned int>(externalTime.count()));

    GetTimeProvider().SetCurrentTime(targetTime);
    Mission.GetState().PersistentState.Set(state::TimeState(targetTime, externalTime));
    GetTerminal().Puts("OK");
}

void AdvanceTimeHandler(uint16_t argc, char* argv[])
{
    if (argc != 1)
    {
        GetTerminal().Puts("advance_time <time>\n");
        return;
    }

    char* tail;
    const auto targetTime = milliseconds(strtoul(argv[0], &tail, 10));
    LOGF(LOG_LEVEL_INFO, "Advancing time by '%lu' seconds\n", static_cast<std::uint32_t>(duration_cast<seconds>(targetTime).count()));
    GetTimeProvider().AdvanceTime(targetTime);
    GetTerminal().Puts("OK");
}

void CurrentTimeHandler(uint16_t argc, char* argv[])
{
    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);
    auto span = GetTimeProvider().GetCurrentTime().Value;
    GetTerminal().Printf("%lu", static_cast<std::uint32_t>(duration_cast<milliseconds>(span).count()));
}
