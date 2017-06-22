#include "BeaconUpdate.hpp"
#include <chrono>
#include "base/IHasState.hpp"
#include "comm/IBeaconController.hpp"
#include "logger/logger.h"
#include "telecommunication/FrameContentWriter.hpp"
#include "telemetry/state.hpp"

namespace mission
{
    using namespace std::chrono_literals;
    using std::chrono::duration_cast;
    using std::chrono::seconds;

    /**
     * @brief Interval between two subsequent beacon updates
     */
    static constexpr std::chrono::milliseconds BeaconUpdateInterval = 30s;

    /**
     * @brief Default beacon send interval.
     */
    static constexpr seconds BeaconInterval = 30s;

    using telecommunication::downlink::FieldId;

    BeaconUpdate::BeaconUpdate(std::pair<devices::comm::IBeaconController&, IHasState<telemetry::TelemetryState>&> arguments)
        : controller(&arguments.first),      //
          telemetryState(&arguments.second), //
          lastBeaconUpdate(0s)
    {
    }

    ActionDescriptor<SystemState> BeaconUpdate::BuildAction()
    {
        ActionDescriptor<SystemState> descriptor;
        descriptor.name = "Beacon Update";
        descriptor.param = this;
        descriptor.condition = ShouldUpdateBeacon;
        descriptor.actionProc = Run;
        return descriptor;
    }

    bool BeaconUpdate::ShouldUpdateBeacon(const SystemState& state, void* param)
    {
        auto This = static_cast<BeaconUpdate*>(param);
        return state.AntennaState.IsDeployed() && //
            ((state.Time - This->lastBeaconUpdate) >= BeaconUpdateInterval);
    }

    void BeaconUpdate::Run(SystemState& state, void* param)
    {
        auto This = static_cast<BeaconUpdate*>(param);
        This->UpdateBeacon(state);
    }

    void BeaconUpdate::UpdateBeacon(const SystemState& state)
    {
        const auto beacon = GenerateBeacon();
        if (!beacon.HasValue)
        {
            return;
        }

        const auto result = this->controller->SetBeacon(beacon.Value);
        const auto time = static_cast<std::uint32_t>(duration_cast<seconds>(state.Time).count());
        if (!result.HasValue)
        {
            LOGF(LOG_LEVEL_INFO, "Beacon update rejected at %lu", time);
        }
        else if (result.Value)
        {
            this->lastBeaconUpdate = state.Time;
            LOGF(LOG_LEVEL_INFO, "Beacon set at %lu", time);
        }
        else
        {
            LOGF(LOG_LEVEL_ERROR, "Unable to set beacon at %lu", time);
        }
    }

    Option<devices::comm::Beacon> BeaconUpdate::GenerateBeacon()
    {
        auto& writer = frame.PayloadWriter();
        auto& telemetry = this->telemetryState->GetState();

        writer.Reset();
        writer.WriteByte(telecommunication::downlink::BeaconMarker);
        {
            Lock lock(telemetry.bufferLock, 5s);
            if (!static_cast<bool>(lock))
            {
                LOG(LOG_LEVEL_ERROR, "[beacon] Unable to acquire access to telemetry.");
                return Option<devices::comm::Beacon>::None();
            }

            writer.WriteArray(telemetry.lastSerializedTelemetry);
        }

        if (!writer.Status())
        {
            LOG(LOG_LEVEL_ERROR, "[beacon] Unable to fit telemetry in single comm frame.");
            return Option<devices::comm::Beacon>::None();
        }

        return Option<devices::comm::Beacon>::Some(BeaconInterval, frame.Frame());
    }
}
