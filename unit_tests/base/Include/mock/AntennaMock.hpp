#ifndef ANTENNA_MOCK_HPP
#define ANTENNA_MOCK_HPP

#pragma once

#include "gmock/gmock.h"
#include "antenna/driver.h"
#include "antenna/telemetry.hpp"

struct AntennaMock : public IAntennaDriver
{
    AntennaMock();
    ~AntennaMock();

    MOCK_METHOD1(Reset, OSResult(AntennaChannel channel));

    MOCK_METHOD0(HardReset, OSResult());

    MOCK_METHOD1(FinishDeployment, OSResult(AntennaChannel channel));

    MOCK_METHOD1(Arm, OSResult(AntennaChannel channel));
    MOCK_METHOD1(Disarm, OSResult(AntennaChannel channel));

    MOCK_METHOD4(DeployAntenna,
        OSResult(AntennaChannel channel,
            AntennaId antennaId,
            std::chrono::milliseconds timeout,
            bool overrideSwitches //
            ));

    MOCK_METHOD2(GetDeploymentStatus, OSResult(AntennaChannel channel, AntennaDeploymentStatus* telemetry));

    MOCK_METHOD2(GetTemperature, OSResult(AntennaChannel channel, uint16_t* temperature));

    MOCK_METHOD1(GetTelemetry, OSResult(devices::antenna::AntennaTelemetry& telemetry));
};

#endif
