#include <array>
#include <cstdio>
#include <unistd.h>
#include "antenna/telemetry.hpp"
#include "base/BitWriter.hpp"
#include "comm/CommTelemetry.hpp"
#include "gyro/telemetry.hpp"
#include "state/time/TimeState.hpp"
#include "system.h"
#include "telemetry/BasicTelemetry.hpp"
#include "telemetry/ErrorCounters.hpp"
#include "telemetry/Experiments.hpp"
#include "telemetry/ImtqTelemetry.hpp"
#include "telemetry/SystemStartup.hpp"
#include "telemetry/Telemetry.hpp"
#include "telemetry/TimeTelemetry.hpp"
#include "telemetry/fwd.hpp"
#include "utils.h"

extern "C" {
extern void initialise_monitor_handles(void);
extern void __libc_init_array(void);
extern int kill(pid_t, int);
}

using namespace telemetry;
using namespace std::chrono_literals;
using namespace devices::antenna;
using namespace devices::comm;
using namespace devices::eps::hk;
using namespace devices::gyro;
using namespace devices::imtq;
using namespace experiments;

AntennaTelemetry GetAntennaTelemetry()
{
    AntennaTelemetry t;

    t.SetActivationCounts(AntennaChannel::ANTENNA_PRIMARY_CHANNEL, ActivationCounts(10, 11, 12, 13));
    t.SetActivationCounts(AntennaChannel::ANTENNA_BACKUP_CHANNEL, ActivationCounts(20, 21, 22, 23));

    t.SetActivationTimes(AntennaChannel::ANTENNA_PRIMARY_CHANNEL, ActivationTimes(30s, 31s, 32s, 33s));
    t.SetActivationTimes(AntennaChannel::ANTENNA_BACKUP_CHANNEL, ActivationTimes(40s, 41s, 42s, 43s));

    t.SetChannelStatus(ANTENNA_PRIMARY_CHANNEL, ChannelStatus::Armed | ChannelStatus::IndependentBurn | ChannelStatus::IgnoringSwitches);
    t.SetChannelStatus(ANTENNA_BACKUP_CHANNEL, ChannelStatus::IndependentBurn);

    t.SetBurningStatus(ANTENNA_PRIMARY_CHANNEL, false, true, true, false);
    t.SetBurningStatus(ANTENNA_BACKUP_CHANNEL, true, false, false, true);

    t.SetTimeReached(ANTENNA_PRIMARY_CHANNEL, true, false, true, false);
    t.SetTimeReached(ANTENNA_BACKUP_CHANNEL, false, true, false, true);

    t.SetDeployedStatus(ANTENNA_PRIMARY_CHANNEL, false, true, false, true);
    t.SetDeployedStatus(ANTENNA_BACKUP_CHANNEL, true, false, true, false);

    return t;
}

CommTelemetry GetCommTelemetry()
{
    TransmitterTelemetry transmitter;
    transmitter.Uptime = 28h + 24min + 45s;
    transmitter.TransmitterBitRate = Bitrate::Comm2400bps;

    transmitter.LastTransmittedRFReflectedPower = 1111;
    transmitter.LastTransmittedAmplifierTemperature = 1222;
    transmitter.LastTransmittedRFForwardPower = 1333;
    transmitter.LastTransmittedTransmitterCurrentConsumption = 1444;

    transmitter.NowRFForwardPower = 1555;
    transmitter.NowTransmitterCurrentConsumption = 1666;

    transmitter.StateWhenIdle = IdleState::On;
    transmitter.BeaconState = false;

    ReceiverTelemetry receiver;
    receiver.Uptime = 28h + 45min + 25s;
    receiver.LastReceivedDopplerOffset = 1777;
    receiver.LastReceivedRSSI = 1888;
    receiver.NowDopplerOffset = 1999;
    receiver.NowReceiverCurrentConsumption = 2000;
    receiver.NowVoltage = 2111;
    receiver.NowOscilatorTemperature = 2222;
    receiver.NowAmplifierTemperature = 2333;
    receiver.NowRSSI = 2444;

    return CommTelemetry(transmitter, receiver);
}

ControllerATelemetry GetControllerA()
{
    ControllerATelemetry hk;

    hk.mpptX.SOL_VOLT = 301;
    hk.mpptX.SOL_CURR = 302;
    hk.mpptX.SOL_OUT_VOLT = 303;
    hk.mpptX.Temperature = 304;
    hk.mpptX.MpptState = MPPT_STATE::FixedPointConversion;

    hk.mpptYPlus.SOL_VOLT = 305;
    hk.mpptYPlus.SOL_CURR = 306;
    hk.mpptYPlus.SOL_OUT_VOLT = 307;
    hk.mpptYPlus.Temperature = 308;
    hk.mpptYPlus.MpptState = MPPT_STATE::NoSolarPanel;

    hk.mpptYMinus.SOL_VOLT = 309;
    hk.mpptYMinus.SOL_CURR = 310;
    hk.mpptYMinus.SOL_OUT_VOLT = 311;
    hk.mpptYMinus.Temperature = 312;
    hk.mpptYMinus.MpptState = MPPT_STATE::MPPTBatteryCharge;

    hk.distr.VOLT_3V3 = 313;
    hk.distr.CURR_3V3 = 314;
    hk.distr.VOLT_5V = 315;
    hk.distr.CURR_5V = 316;
    hk.distr.VOLT_VBAT = 317;
    hk.distr.CURR_VBAT = 318;
    hk.distr.LCL_STATE =
        static_cast<DISTR_LCL_STATE>(num(DISTR_LCL_STATE::CamNadir) | num(DISTR_LCL_STATE::SENS) | num(DISTR_LCL_STATE::IMTQ));
    hk.distr.LCL_FLAGB = static_cast<DISTR_LCL_FLAGB>(num(DISTR_LCL_FLAGB::CamNadir) | num(DISTR_LCL_FLAGB::SENS));

    hk.batc.VOLT_A = 319;
    hk.batc.ChargeCurrent = 320;
    hk.batc.DischargeCurrent = 321;
    hk.batc.Temperature = 322;
    hk.batc.State = BATC_STATE::C;

    hk.bp.temperatureA = 323;
    hk.bp.temperatureB = 324;

    hk.current.safetyCounter = 225;
    hk.current.powerCycleCount = 326;
    hk.current.uptime = 327;
    hk.current.temperature = 328;
    hk.current.suppTemp = 329;

    hk.other.VOLT_3V3d = 330;

    hk.dcdc3V3.temperature = 331;
    hk.dcdc5V.temperature = 332;

    return hk;
}

ControllerBTelemetry GetControllerB()
{
    ControllerBTelemetry hk;

    hk.bp.temperatureC = 333;

    hk.batc.voltB = 334;

    hk.current.safetyCounter = 235;
    hk.current.powerCycleCount = 336;
    hk.current.uptime = 337;
    hk.current.temperature = 338;
    hk.current.suppTemp = 339;

    hk.other.VOLT_3V3d = 340;

    return hk;
}

void FillTelemetry(ManagedTelemetry& telemetry)
{
    telemetry.Set(SystemStartup(0xDEADBEEF, 0b111, 0x5));
    telemetry.Set(ProgramState(0x1122));
    telemetry.Set(InternalTimeTelemetry(1234s));
    telemetry.Set(ExternalTimeTelemetry(4321s));
    telemetry.Set(ErrorCountingTelemetry({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12}));
    telemetry.Set(FlashPrimarySlotsScrubbing(0b101));
    telemetry.Set(FlashSecondarySlotsScrubbing(0b010));
    telemetry.Set(RAMScrubbing(11223344));
    telemetry.Set(OSState(123456));
    telemetry.Set(FileSystemTelemetry(44332211));
    telemetry.Set(GetAntennaTelemetry());
    telemetry.Set(ExperimentTelemetry(10, StartResult::Failure, IterationResult::LoopImmediately));
    telemetry.Set(GyroscopeTelemetry(350, -4023, 352, 353));
    telemetry.Set(GetCommTelemetry());
    telemetry.Set(GpioState(true));
    telemetry.Set(McuTemperature(60));
    telemetry.Set(GetControllerA());
    telemetry.Set(GetControllerB());
    telemetry.Set(ImtqMagnetometerMeasurements(std::array<devices::imtq::MagnetometerMeasurement, 3>{-55667788, 55778866, 55886677}));
    telemetry.Set(ImtqCoilsActive(true));
    telemetry.Set(ImtqDipoles(std::array<devices::imtq::Dipole, 3>{501, -502, 503}));
    telemetry.Set(ImtqBDotTelemetry(std::array<devices::imtq::BDotType, 3>{99667788, 99778866, -99886677}));
    telemetry.Set(ImtqHousekeeping(520, 521, 522, 523, 524));
    telemetry.Set(ImtqCoilCurrent(std::array<devices::imtq::Dipole, 3>{531, 532, 533}));
    telemetry.Set(ImtqCoilTemperature(std::array<devices::imtq::Dipole, 3>{541, 542, 543}));
    telemetry.Set(ImtqStatus(67));
    telemetry.Set(ImtqState(Mode::Selftest, 56, true, 45s));
    telemetry.Set(ImtqSelfTest(std::array<std::uint8_t, 8>{80, 81, 82, 83, 84, 85, 86, 87}));
}

void WriteTelemetryToFile()
{
    auto f = fopen("telemetry", "wb");

    ManagedTelemetry telemetry;

    FillTelemetry(telemetry);

    std::array<std::uint8_t, decltype(telemetry)::TotalSerializedSize> buf;

    BitWriter writer(buf);
    telemetry.Write(writer);

    fwrite(buf.data(), 1, buf.size(), f);

    fclose(f);
}

int main()
{
    __libc_init_array();

    initialise_monitor_handles();

    WriteTelemetryToFile();

    kill(-1, 0);

    return 0;
}
