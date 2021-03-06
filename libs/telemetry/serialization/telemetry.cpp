#include "mission/telemetry.hpp"
#include <cassert>
#include "base/BitWriter.hpp"
#include "logger/logger.h"
#include "telemetry/state.hpp"

namespace mission
{
    using namespace std::chrono_literals;
    using services::fs::SeekOrigin;

    TelemetryTask::TelemetryTask(std::tuple<services::fs::IFileSystem&, TelemetryConfiguration> arguments)
        : provider(std::get<0>(arguments)),      //
          configuration(std::get<1>(arguments)), //
          delay(configuration.delay),            //
          lastTelemetrySave(0ms)
    {
    }

    ActionDescriptor<telemetry::TelemetryState> TelemetryTask::BuildAction()
    {
        ActionDescriptor<telemetry::TelemetryState> descriptor;
        descriptor.name = "Save telemetry to file";
        descriptor.param = this;
        descriptor.condition = SaveCondition;
        descriptor.actionProc = SaveProxy;
        return descriptor;
    }

    bool TelemetryTask::SaveCondition(const telemetry::TelemetryState& state, void* param)
    {
        auto This = static_cast<TelemetryTask*>(param);
        auto time = state.telemetry.Get<telemetry::InternalTimeTelemetry>();

        auto delta = time.Time() - This->lastTelemetrySave;

        return (delta < 0ms) || (delta >= This->delay);
    }

    void TelemetryTask::SaveProxy(telemetry::TelemetryState& state, void* param)
    {
        auto This = static_cast<TelemetryTask*>(param);
        This->Save(state);
    }

    void TelemetryTask::Save(telemetry::TelemetryState& stateObject)
    {
        decltype(telemetry::TelemetryState::lastSerializedTelemetry) content;

        {
            Lock lock(stateObject.bufferLock, 5s);
            if (static_cast<bool>(lock))
            {
                memcpy(content.data(), stateObject.lastSerializedTelemetry.data(), content.size());
            }
            else
            {
                LOG(LOG_LEVEL_WARNING, "Unable to acquire access to serialized telemetry. ");
                return;
            }
        }

        if (SaveToFile(content))
        {
            auto time = stateObject.telemetry.Get<telemetry::InternalTimeTelemetry>();
            this->lastTelemetrySave = time.Time();
        }
    }

    static inline std::size_t CalculateBestOffset(std::size_t currentSize)
    {
        auto rem = currentSize % TelemetryTask::AlignFileEntriesTo;

        if (rem == 0)
        {
            return currentSize;
        }

        return currentSize + (TelemetryTask::AlignFileEntriesTo - rem);
    }

    bool TelemetryTask::SaveToFile(gsl::span<const std::uint8_t> buffer)
    {
        services::fs::File file(this->provider, //
            this->configuration.currentFileName,
            services::fs::FileOpen::OpenAlways,
            services::fs::FileAccess::WriteOnly);
        if (!file)
        {
            LOGF(LOG_LEVEL_ERROR, "Unable to open telemetry file: '%s'.", this->configuration.currentFileName);
            return false;
        }

        auto size = file.Size();
        if (size >= this->configuration.maxFileSize)
        {
            file.Close();

            const auto status = this->provider.Move(this->configuration.currentFileName, this->configuration.previousFileName);
            if (OS_RESULT_FAILED(status))
            {
                LOGF(LOG_LEVEL_ERROR,
                    "Unable to archive telemetry file: '%s' as '%s'.",
                    this->configuration.currentFileName,
                    this->configuration.previousFileName);
                return false;
            }

            file = services::fs::File(this->provider,
                this->configuration.currentFileName,
                services::fs::FileOpen::CreateAlways,
                services::fs::FileAccess::WriteOnly);
            if (!file)
            {
                LOGF(LOG_LEVEL_ERROR, "Unable to open telemetry file: '%s'.", this->configuration.currentFileName);
                return false;
            }

            size = file.Size();
        }

        file.Seek(SeekOrigin::Begin, CalculateBestOffset(size));

        return static_cast<bool>(file.Write(buffer));
    }
}
