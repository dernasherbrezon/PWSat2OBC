#ifndef LIBS_TELEMETRY_FWD_HPP
#define LIBS_TELEMETRY_FWD_HPP

#pragma once

#include "comm/comm.hpp"
#include "gyro/gyro.h"
#include "state/fwd.hpp"

namespace telemetry
{
    class SystemStartup;
    class ProgramState;
    class ErrorCountingTelemetry;

    template <typename... Type> class Telemetry;

    typedef Telemetry<SystemStartup,      //
        ProgramState,                     //
        state::TimeState,                 //
        ErrorCountingTelemetry,           //
        devices::comm::CommTelemetry,     //
        devices::gyro::GyroscopeTelemetry //
        >
        ManagedTelemetry;
}

#endif