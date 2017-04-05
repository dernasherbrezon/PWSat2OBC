#include <algorithm>
#include <gsl/span>
#include <string>
#include <tuple>
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "gmock/gmock-matchers.h"
#include "I2C/I2CMock.hpp"
#include "OsMock.hpp"
#include "comm/Beacon.hpp"
#include "comm/CommDriver.hpp"
#include "comm/Frame.hpp"
#include "comm/IHandleFrame.hpp"
#include "I2C/I2CMock.hpp"
#include "i2c/i2c.h"
#include "mock/error_counter.hpp"
#include "os/os.hpp"
#include "system.h"
#include "utils.hpp"

using testing::_;
using testing::Eq;
using testing::Ne;
using testing::Ge;
using testing::StrEq;
using testing::Return;
using testing::Invoke;
using testing::Pointee;
using testing::ElementsAre;
using testing::Matches;
using gsl::span;
using drivers::i2c::I2CResult;
using namespace devices::comm;
using namespace std::chrono_literals;

static constexpr uint8_t ReceiverAddress = 0x60;
static constexpr uint8_t TransmitterAddress = 0x61;

static constexpr uint8_t ReceverGetTelemetry = 0x1A;
static constexpr uint8_t ReceiverGetFrameCount = 0x21;
static constexpr uint8_t ReceiverGetFrame = 0x22;
static constexpr uint8_t ReceiverRemoveFrame = 0x24;
static constexpr uint8_t ReceiverReset = 0xAA;
static constexpr uint8_t ReceiverWatchdogReset = 0xCC;

static constexpr uint8_t HardwareReset = 0xAB;

static constexpr uint8_t TransmitterSendFrame = 0x10;
static constexpr uint8_t TransmitterSetBeacon = 0x14;
static constexpr uint8_t TransmitterClearBeacon = 0x1f;
static constexpr uint8_t TransmitterSetIdleState = 0x24;
static constexpr uint8_t TransmitterGetTelemetry = 0x26;
static constexpr uint8_t TransmitterSetBitrate = 0x28;
static constexpr uint8_t TransmitterGetState = 0x41;
static constexpr uint8_t TransmitterReset = 0xAA;
static constexpr uint8_t TransmitterWatchdogReset = 0xCC;

static OSReset SetupOs(CommObject& comm, OSMock& system)
{
    auto reset = InstallProxy(&system);
    ON_CALL(system, CreateEventGroup()).WillByDefault(Return(reinterpret_cast<OSEventGroupHandle>(&comm)));

    EXPECT_THAT(comm.Initialize(), Eq(OSResult::Success));

    return reset;
}

struct FrameHandlerMock : IHandleFrame
{
    MOCK_METHOD2(HandleFrame, void(ITransmitFrame&, Frame&));
};

struct CommTest : public testing::Test
{
    CommTest();
    CommObject comm;
    FrameHandlerMock frameHandler;
    testing::NiceMock<OSMock> system;
    I2CBusMock i2c;
    OSReset reset;
    std::uint8_t dataBuffer[devices::comm::PrefferedBufferSize];
    ErrorCountingConfigrationMock errorsConfig;
    error_counter::ErrorCounting errors;
    error_counter::ErrorCounter<1> error_counter;

    void MockFrameCount(std::uint16_t frameCount);

    void MockFrame(gsl::span<const std::uint8_t> span, std::uint16_t rssi = 0, std::uint16_t doppler = 0);

    void MockRemoveFrame(I2CResult result);
};

CommTest::CommTest() : comm(errors, i2c, frameHandler), errors(errorsConfig), error_counter(errors)
{
    reset = SetupOs(comm, system);
}

void CommTest::MockFrameCount(std::uint16_t frameCount)
{
    EXPECT_CALL(i2c, WriteRead(ReceiverAddress, ElementsAre(ReceiverGetFrameCount), _))
        .WillOnce(Invoke([=](uint8_t /*address*/, auto /*inData*/, auto outData) {
            outData[0] = frameCount & 0xff;
            outData[1] = (frameCount >> 8) & 0xff;
            return I2CResult::OK;
        }));
}

void CommTest::MockFrame(gsl::span<const std::uint8_t> data, std::uint16_t rssi, std::uint16_t doppler)
{
    EXPECT_CALL(i2c, WriteRead(ReceiverAddress, ElementsAre(ReceiverGetFrame), _))
        .WillRepeatedly(Invoke([=](uint8_t /*address*/, gsl::span<const uint8_t> /*inData*/, gsl::span<uint8_t> outData) {
            std::fill(outData.begin(), outData.end(), 0);
            outData[0] = data.size_bytes() & 0xff;
            outData[1] = (data.size_bytes() >> 8) & 0xff;
            if (outData.size() > (5 + data.size_bytes()))
            {
                outData[2] = doppler & 0xff;
                outData[3] = (doppler >> 8) & 0xff;
                outData[4] = rssi & 0xff;
                outData[5] = (rssi >> 8) & 0xff;
                std::copy(std::begin(data), std::end(data), outData.begin() + 6);
            }

            return I2CResult::OK;
        }));
}

void CommTest::MockRemoveFrame(I2CResult result)
{
    i2c.ExpectWriteCommand(ReceiverAddress, ReceiverRemoveFrame).WillOnce(Return(result));
}

TEST_F(CommTest, TestInitializationDoesNotTouchHardware)
{
    CommObject commObject(errors, i2c, frameHandler);

    EXPECT_CALL(i2c, WriteRead(_, _, _)).Times(0);
    EXPECT_CALL(i2c, Write(_, _)).Times(0);

    commObject.Initialize();
}

TEST_F(CommTest, TestInitializationAllocationFailure)
{
    CommObject commObject(errors, i2c, frameHandler);

    EXPECT_CALL(system, CreateEventGroup()).WillOnce(Return(nullptr));

    const auto status = commObject.Initialize();

    ASSERT_THAT(status, Ne(OSResult::Success));
}

TEST_F(CommTest, TestInitialization)
{
    CommObject commObject(errors, i2c, frameHandler);

    const auto status = commObject.Initialize();

    ASSERT_THAT(status, Eq(OSResult::Success));
}

TEST_F(CommTest, TestHardwareReset)
{
    i2c.ExpectWriteCommand(ReceiverAddress, HardwareReset).WillOnce(Return(I2CResult::OK));
    const auto status = comm.Reset();
    ASSERT_THAT(status, Eq(true));
}

TEST_F(CommTest, TestHardwareResetFailureOnHardware)
{
    i2c.ExpectWriteCommand(ReceiverAddress, HardwareReset).WillOnce(Return(I2CResult::Nack));
    const auto status = comm.Reset();
    ASSERT_THAT(status, Eq(false));
    ASSERT_THAT(error_counter, Eq(5));
}

TEST_F(CommTest, TestTransmitterReset)
{
    i2c.ExpectWriteCommand(TransmitterAddress, TransmitterReset).WillOnce(Return(I2CResult::OK));
    const auto status = comm.ResetTransmitter();
    ASSERT_THAT(status, Eq(true));
}

TEST_F(CommTest, TestTransmitterResetFailure)
{
    i2c.ExpectWriteCommand(TransmitterAddress, TransmitterReset).WillOnce(Return(I2CResult::Nack));
    const auto status = comm.ResetTransmitter();
    ASSERT_THAT(status, Eq(false));
}

TEST_F(CommTest, TestReceiverReset)
{
    i2c.ExpectWriteCommand(ReceiverAddress, ReceiverReset).WillOnce(Return(I2CResult::OK));
    const auto status = comm.ResetReceiver();
    ASSERT_THAT(status, Eq(true));
}

TEST_F(CommTest, TestReceiverResetFailure)
{
    i2c.ExpectWriteCommand(ReceiverAddress, ReceiverReset).WillOnce(Return(I2CResult::Nack));
    const auto status = comm.ResetReceiver();
    ASSERT_THAT(status, Eq(false));
}

TEST_F(CommTest, TestFrameRemoval)
{
    MockRemoveFrame(I2CResult::OK);
    const auto status = comm.RemoveFrame();
    ASSERT_THAT(status, Eq(true));
}

TEST_F(CommTest, TestFrameRemovalFailure)
{
    MockRemoveFrame(I2CResult::Nack);
    const auto status = comm.RemoveFrame();
    ASSERT_THAT(status, Eq(false));
}

TEST_F(CommTest, TestGetFrameCountFailure)
{
    EXPECT_CALL(i2c, WriteRead(ReceiverAddress, ElementsAre(ReceiverGetFrameCount), _)).WillOnce(Return(I2CResult::Nack));
    const auto result = comm.GetFrameCount();
    ASSERT_THAT(result.status, Eq(false));
    ASSERT_THAT(result.frameCount, Eq(0));
}

TEST_F(CommTest, TestGetFrameCount)
{
    MockFrameCount(257);
    const auto result = comm.GetFrameCount();
    ASSERT_THAT(result.status, Eq(true));
    ASSERT_THAT(result.frameCount, Eq(257));
}

TEST_F(CommTest, TestClearBeaconFailure)
{
    i2c.ExpectWriteCommand(TransmitterAddress, TransmitterClearBeacon).WillOnce(Return(I2CResult::Nack));
    const auto status = comm.ClearBeacon();
    ASSERT_THAT(status, Eq(false));
}

TEST_F(CommTest, TestClearBeacon)
{
    i2c.ExpectWriteCommand(TransmitterAddress, TransmitterClearBeacon).WillOnce(Return(I2CResult::OK));
    const auto status = comm.ClearBeacon();
    ASSERT_THAT(status, Eq(true));
}

TEST_F(CommTest, TestSetIdleStateFailure)
{
    EXPECT_CALL(i2c, Write(TransmitterAddress, ElementsAre(TransmitterSetIdleState, _))).WillOnce(Return(I2CResult::Nack));
    const auto status = comm.SetTransmitterStateWhenIdle(IdleState::On);
    ASSERT_THAT(status, Eq(false));
}

TEST_F(CommTest, TestSetIdleState)
{
    EXPECT_CALL(i2c, Write(TransmitterAddress, ElementsAre(TransmitterSetIdleState, _))).WillOnce(Return(I2CResult::OK));
    const auto status = comm.SetTransmitterStateWhenIdle(IdleState::On);
    ASSERT_THAT(status, Eq(true));
}

TEST_F(CommTest, TestSetIdleStateandOn)
{
    EXPECT_CALL(i2c, Write(TransmitterAddress, ElementsAre(TransmitterSetIdleState, _)))
        .WillOnce(Invoke([](uint8_t /*address*/, auto inData) {
            EXPECT_THAT(inData[1], Eq(1));
            return I2CResult::OK;
        }));
    const auto status = comm.SetTransmitterStateWhenIdle(IdleState::On);
    ASSERT_THAT(status, Eq(true));
}

TEST_F(CommTest, TestSetIdleStateandOff)
{
    EXPECT_CALL(i2c, Write(TransmitterAddress, ElementsAre(TransmitterSetIdleState, _)))
        .WillOnce(Invoke([](uint8_t /*address*/, auto inData) {
            EXPECT_THAT(inData[1], Eq(0));
            return I2CResult::OK;
        }));
    const auto status = comm.SetTransmitterStateWhenIdle(IdleState::Off);
    ASSERT_THAT(status, Eq(true));
}

TEST_F(CommTest, TestSetBitrateFailure)
{
    EXPECT_CALL(i2c, Write(TransmitterAddress, ElementsAre(TransmitterSetBitrate, _))).WillOnce(Return(I2CResult::Nack));
    const auto status = comm.SetTransmitterBitRate(Bitrate::Comm1200bps);
    ASSERT_THAT(status, Eq(false));
}

TEST_F(CommTest, TestSetBitrate)
{
    EXPECT_CALL(i2c, Write(TransmitterAddress, ElementsAre(TransmitterSetBitrate, _))).WillOnce(Return(I2CResult::OK));
    const auto status = comm.SetTransmitterBitRate(Bitrate::Comm1200bps);
    ASSERT_THAT(status, Eq(true));
}

TEST_F(CommTest, TestSetBitrateand)
{
    EXPECT_CALL(i2c, Write(TransmitterAddress, ElementsAre(TransmitterSetBitrate, _)))
        .WillOnce(Invoke([](uint8_t /*address*/, auto inData) {
            EXPECT_THAT(inData[1], Eq(1));
            return I2CResult::OK;
        }))
        .WillOnce(Invoke([](uint8_t /*address*/, auto inData) {
            EXPECT_THAT(inData[1], Eq(2));
            return I2CResult::OK;
        }))
        .WillOnce(Invoke([](uint8_t /*address*/, auto inData) {
            EXPECT_THAT(inData[1], Eq(4));
            return I2CResult::OK;
        }))
        .WillOnce(Invoke([](uint8_t /*address*/, auto inData) {
            EXPECT_THAT(inData[1], Eq(8));
            return I2CResult::OK;
        }));
    auto status = comm.SetTransmitterBitRate(Bitrate::Comm1200bps);
    ASSERT_THAT(status, Eq(true));
    status = comm.SetTransmitterBitRate(Bitrate::Comm2400bps);
    ASSERT_THAT(status, Eq(true));
    status = comm.SetTransmitterBitRate(Bitrate::Comm4800bps);
    ASSERT_THAT(status, Eq(true));
    status = comm.SetTransmitterBitRate(Bitrate::Comm9600bps);
    ASSERT_THAT(status, Eq(true));
}

TEST_F(CommTest, TestGetTransmitterStateFailure)
{
    TransmitterState state;
    EXPECT_CALL(i2c, WriteRead(TransmitterAddress, ElementsAre(TransmitterGetState), _)).WillOnce(Return(I2CResult::Nack));
    const auto status = comm.GetTransmitterState(state);
    ASSERT_THAT(status, Eq(false));
}

TEST_F(CommTest, TestGetTransmitterInvalidResponse)
{
    TransmitterState state;
    EXPECT_CALL(i2c, WriteRead(TransmitterAddress, ElementsAre(TransmitterGetState), _))
        .WillOnce(Invoke([](uint8_t /*address*/, auto /*inData*/, auto outData) {
            outData[0] = 0xff;
            return I2CResult::OK;
        }));
    const auto status = comm.GetTransmitterState(state);
    ASSERT_THAT(status, Eq(false));
}

TEST_F(CommTest, TestGetTransmitterResponse)
{
    TransmitterState state;
    EXPECT_CALL(i2c, WriteRead(TransmitterAddress, ElementsAre(TransmitterGetState), _))
        .WillOnce(Invoke([](uint8_t /*address*/, auto /*inData*/, auto outData) {
            outData[0] = 0x7f;
            return I2CResult::OK;
        }));
    const auto status = comm.GetTransmitterState(state);
    ASSERT_THAT(status, Eq(true));
    ASSERT_THAT(state.BeaconState, Eq(true));
    ASSERT_THAT(state.StateWhenIdle, Eq(IdleState::On));
    ASSERT_THAT(state.TransmitterBitRate, Eq(Bitrate::Comm9600bps));
}

TEST_F(CommTest, TestGetBaseLineTransmitterResponse)
{
    TransmitterState state;
    EXPECT_CALL(i2c, WriteRead(TransmitterAddress, ElementsAre(TransmitterGetState), _))
        .WillOnce(Invoke([](uint8_t /*address*/, auto /*inData*/, auto outData) {
            outData[0] = 0x0;
            return I2CResult::OK;
        }));
    const auto status = comm.GetTransmitterState(state);
    ASSERT_THAT(status, Eq(true));
    ASSERT_THAT(state.BeaconState, Eq(false));
    ASSERT_THAT(state.StateWhenIdle, Eq(IdleState::Off));
    ASSERT_THAT(state.TransmitterBitRate, Eq(Bitrate::Comm1200bps));
}

TEST_F(CommTest, TestGetMixedLineTransmitterResponse)
{
    TransmitterState state;
    EXPECT_CALL(i2c, WriteRead(TransmitterAddress, ElementsAre(TransmitterGetState), _))
        .WillOnce(Invoke([](uint8_t /*address*/, auto /*inData*/, auto outData) {
            outData[0] = 0x0a;
            return I2CResult::OK;
        }));
    const auto status = comm.GetTransmitterState(state);
    ASSERT_THAT(status, Eq(true));
    ASSERT_THAT(state.BeaconState, Eq(true));
    ASSERT_THAT(state.StateWhenIdle, Eq(IdleState::Off));
    ASSERT_THAT(state.TransmitterBitRate, Eq(Bitrate::Comm4800bps));
}

TEST_F(CommTest, TestSendTooLongFrame)
{
    uint8_t buffer[devices::comm::MaxDownlinkFrameSize + 1] = {0};
    const auto status = comm.SendFrame(buffer);
    ASSERT_THAT(status, Eq(false));
}

TEST_F(CommTest, TestSendFrameFailure)
{
    uint8_t buffer[10] = {0};
    EXPECT_CALL(i2c, WriteRead(TransmitterAddress, BeginsWith(TransmitterSendFrame), _)).WillOnce(Return(I2CResult::Nack));
    const auto status = comm.SendFrame(span<const uint8_t, 10>(buffer));
    ASSERT_THAT(status, Eq(false));
}

TEST_F(CommTest, TestSendFrame)
{
    uint8_t buffer[] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc};
    EXPECT_CALL(i2c, WriteRead(TransmitterAddress, BeginsWith(TransmitterSendFrame), _))
        .WillOnce(Invoke([](uint8_t /*address*/, span<const uint8_t> inData, span<uint8_t> outData) {
            const uint8_t expected[] = {TransmitterSendFrame, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc};
            EXPECT_THAT(inData, Eq(span<const uint8_t>(expected)));

            outData[0] = 0;
            return I2CResult::OK;
        }));
    const auto status = comm.SendFrame(span<const uint8_t>(buffer));
    ASSERT_THAT(status, Eq(true));
}

TEST_F(CommTest, TestSendFrameRejectedByHardware)
{
    uint8_t buffer[] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc};
    EXPECT_CALL(i2c, WriteRead(TransmitterAddress, BeginsWith(TransmitterSendFrame), _))
        .WillOnce(Invoke([](uint8_t /*address*/, span<const uint8_t> /*inData*/, span<uint8_t> outData) {
            outData[0] = 0xff;
            return I2CResult::OK;
        }));
    const auto status = comm.SendFrame(span<const uint8_t>(buffer));
    ASSERT_THAT(status, Eq(false));
}

TEST_F(CommTest, TestReceiveFrameFailure)
{
    Frame frame;
    EXPECT_CALL(i2c, WriteRead(ReceiverAddress, ElementsAre(ReceiverGetFrame), _)).WillOnce(Return(I2CResult::Nack));
    const auto status = comm.ReceiveFrame(dataBuffer, frame);
    ASSERT_THAT(status, Eq(false));
}

TEST_F(CommTest, TestReceiveFrameTooShortBuffer)
{
    std::uint8_t buffer[1];
    Frame frame;
    EXPECT_CALL(i2c, WriteRead(ReceiverAddress, ElementsAre(ReceiverGetFrame), _)).Times(0);
    const auto status = comm.ReceiveFrame(buffer, frame);
    ASSERT_THAT(status, Eq(false));
}

TEST_F(CommTest, TestReceiveFramePartialHeader)
{
    std::uint8_t buffer[5];
    Frame frame;
    EXPECT_CALL(i2c, WriteRead(ReceiverAddress, ElementsAre(ReceiverGetFrame), _))
        .WillRepeatedly(Invoke([&](uint8_t /*address*/, span<const uint8_t> /*inData*/, span<uint8_t> outData) {
            std::fill(outData.begin(), outData.end(), 0);
            outData[0] = 32;
            if (static_cast<size_t>(outData.size()) > 4)
            {
                outData[2] = 0xab;
                outData[3] = 0x0c;
                outData[4] = 0xde;
            }

            return I2CResult::OK;
        }));
    const auto status = comm.ReceiveFrame(buffer, frame);
    ASSERT_THAT(status, Eq(true));
    ASSERT_THAT(frame.FullSize(), Eq(32));
    ASSERT_THAT(frame.Doppler(), Eq(0xcab));
    ASSERT_THAT(frame.Rssi(), Eq(0));
    ASSERT_THAT(frame.Payload().empty(), Eq(true));
}

TEST_F(CommTest, TestReceiveFrameReceiveFailure)
{
    Frame frame;
    EXPECT_CALL(i2c, WriteRead(ReceiverAddress, ElementsAre(ReceiverGetFrame), _))
        .WillOnce(Invoke([](uint8_t /*address*/, span<const uint8_t> /*inData*/, span<uint8_t> outData) {
            std::fill(outData.begin(), outData.end(), 0);
            outData[0] = 1;
            return I2CResult::OK;
        }))
        .WillOnce(Return(I2CResult::Nack));
    const auto status = comm.ReceiveFrame(dataBuffer, frame);
    ASSERT_THAT(status, Eq(false));
}

TEST_F(CommTest, TestReceiveFrame)
{
    Frame frame;
    const uint8_t expected[] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
    MockFrame(expected, 0xdde, 0xcab);
    const auto status = comm.ReceiveFrame(dataBuffer, frame);
    ASSERT_THAT(status, Eq(true));
    ASSERT_THAT(frame.Size(), Eq(COUNT_OF(expected)));
    ASSERT_THAT(frame.Doppler(), Eq(0xcab));
    ASSERT_THAT(frame.Rssi(), Eq(0xdde));
    ASSERT_THAT(gsl::span<const std::uint8_t>(frame.Payload()), Eq(span<const uint8_t>(expected)));
}

TEST_F(CommTest, TestReceiveFramePartialData)
{
    std::uint8_t buffer[22];
    Frame frame;
    const uint8_t expected[] = {0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
    EXPECT_CALL(i2c, WriteRead(ReceiverAddress, ElementsAre(ReceiverGetFrame), _))
        .WillRepeatedly(Invoke([&](uint8_t /*address*/, span<const uint8_t> /*inData*/, span<uint8_t> outData) {
            std::fill(outData.begin(), outData.end(), 0);
            outData[0] = 32;
            if (static_cast<size_t>(outData.size()) > 5)
            {
                outData[2] = 0xab;
                outData[3] = 0x0c;
                outData[4] = 0xde;
                outData[5] = 0x0d;
            }

            auto contentSize = std::min<ptrdiff_t>(outData.size() - 6, count_of(expected));
            if (contentSize > 0)
            {
                std::copy(std::begin(expected), std::begin(expected) + contentSize, outData.begin() + 6);
            }

            return I2CResult::OK;
        }));
    const auto status = comm.ReceiveFrame(buffer, frame);
    ASSERT_THAT(status, Eq(true));
    ASSERT_THAT(frame.Size(), Eq(16));
    ASSERT_THAT(frame.FullSize(), Eq(32));
    ASSERT_THAT(frame.Doppler(), Eq(0xcab));
    ASSERT_THAT(frame.Rssi(), Eq(0xdde));
    ASSERT_THAT(gsl::span<const std::uint8_t>(frame.Payload()), Eq(span<const uint8_t>(expected)));
}

TEST_F(CommTest, TestReceiverTelemetry)
{
    ReceiverTelemetry telemetry;
    const uint8_t expected[] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e};
    EXPECT_CALL(i2c, WriteRead(ReceiverAddress, ElementsAre(ReceverGetTelemetry), _))
        .WillOnce(Invoke([&](uint8_t /*address*/, span<const uint8_t> /*inData*/, span<uint8_t> outData) {
            std::fill(outData.begin(), outData.end(), 0);
            std::copy(std::begin(expected), std::end(expected), outData.begin());
            return I2CResult::OK;
        }));
    const auto status = comm.GetReceiverTelemetry(telemetry);
    ASSERT_THAT(status, Eq(true));
    ASSERT_THAT(telemetry.TransmitterCurrentConsumption, Eq(0x0201));
    ASSERT_THAT(telemetry.DopplerOffset, Eq(0x0403));
    ASSERT_THAT(telemetry.ReceiverCurrentConsumption, Eq(0x0605));
    ASSERT_THAT(telemetry.Vcc, Eq(0x0807));
    ASSERT_THAT(telemetry.OscilatorTemperature, Eq(0x0a09));
    ASSERT_THAT(telemetry.AmplifierTemperature, Eq(0x0c0b));
    ASSERT_THAT(telemetry.SignalStrength, Eq(0x0e0d));
}

TEST_F(CommTest, TestTransmitterTelemetry)
{
    TransmitterTelemetry telemetry;
    const uint8_t expected[] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8};
    EXPECT_CALL(i2c, WriteRead(TransmitterAddress, ElementsAre(TransmitterGetTelemetry), _))
        .WillOnce(Invoke([&](uint8_t /*address*/, span<const uint8_t> /*inData*/, span<uint8_t> outData) {
            std::fill(outData.begin(), outData.end(), 0);
            std::copy(std::begin(expected), std::end(expected), outData.begin());
            return I2CResult::OK;
        }));
    const auto status = comm.GetTransmitterTelemetry(telemetry);
    ASSERT_THAT(status, Eq(true));
    ASSERT_THAT(telemetry.RFReflectedPower, Eq(0x0201));
    ASSERT_THAT(telemetry.AmplifierTemperature, Eq(0x0403));
    ASSERT_THAT(telemetry.RFForwardPower, Eq(0x0605));
    ASSERT_THAT(telemetry.TransmitterCurrentConsumption, Eq(0x0807));
}

TEST_F(CommTest, TestSetBeaconTransmittFailure)
{
    std::uint8_t buffer[1];
    Beacon beacon(1s, buffer);
    EXPECT_CALL(i2c, WriteRead(TransmitterAddress, _, _)).WillOnce(Return(I2CResult::Failure));
    const auto status = comm.SetBeacon(beacon);
    ASSERT_THAT(status, Eq(false));
}

TEST_F(CommTest, TestSetBeaconTransmitterRejected)
{
    EXPECT_CALL(i2c, Write(TransmitterAddress, BeginsWith(TransmitterSetBeacon))).Times(0);
    EXPECT_CALL(i2c, WriteRead(TransmitterAddress, BeginsWith(TransmitterSendFrame), _))
        .WillOnce(Invoke([](uint8_t /*address*/, span<const uint8_t> /*inData*/, span<uint8_t> outData) {
            outData[0] = 0xff;
            return I2CResult::OK;
        }));

    std::uint8_t buffer[1];
    Beacon beacon(1s, buffer);
    const auto status = comm.SetBeacon(beacon);
    ASSERT_THAT(status, Eq(false));
}

TEST_F(CommTest, TestSetBeaconTransmitterBufferNotEmpty)
{
    EXPECT_CALL(i2c, Write(TransmitterAddress, BeginsWith(TransmitterSetBeacon))).Times(0);
    EXPECT_CALL(i2c, WriteRead(TransmitterAddress, BeginsWith(TransmitterSendFrame), _))
        .WillOnce(Invoke([](uint8_t /*address*/, span<const uint8_t> /*inData*/, span<uint8_t> outData) {
            outData[0] = 38;
            return I2CResult::OK;
        }));

    std::uint8_t buffer[1];
    Beacon beacon(1s, buffer);
    const auto status = comm.SetBeacon(beacon);
    ASSERT_THAT(status.HasValue, Eq(false));
}

TEST_F(CommTest, TestSetBeaconTransmitterBufferFull)
{
    EXPECT_CALL(i2c, Write(TransmitterAddress, BeginsWith(TransmitterSetBeacon))).Times(0);
    EXPECT_CALL(i2c, WriteRead(TransmitterAddress, BeginsWith(TransmitterSendFrame), _))
        .WillOnce(Invoke([](uint8_t /*address*/, span<const uint8_t> /*inData*/, span<uint8_t> outData) {
            outData[0] = 0;
            return I2CResult::OK;
        }));

    std::uint8_t buffer[1];
    Beacon beacon(1s, buffer);
    const auto status = comm.SetBeacon(beacon);
    ASSERT_THAT(status.HasValue, Eq(false));
}

TEST_F(CommTest, TestSetBeaconFailure)
{
    EXPECT_CALL(i2c, WriteRead(TransmitterAddress, BeginsWith(TransmitterSendFrame), _))
        .WillOnce(Invoke([](uint8_t /*address*/, span<const uint8_t> /*inData*/, span<uint8_t> outData) {
            outData[0] = 39;
            return I2CResult::OK;
        }));
    EXPECT_CALL(i2c, Write(TransmitterAddress, BeginsWith(TransmitterSetBeacon))).WillOnce(Return(I2CResult::Nack));

    std::uint8_t buffer[1];
    Beacon beacon(1s, buffer);
    const auto status = comm.SetBeacon(beacon);
    ASSERT_THAT(status, Eq(false));
}

TEST_F(CommTest, TestSetBeaconSizeOutOfRange)
{
    EXPECT_CALL(i2c, Write(TransmitterAddress, BeginsWith(TransmitterSetBeacon))).Times(0);
    EXPECT_CALL(i2c, WriteRead(TransmitterAddress, BeginsWith(TransmitterSendFrame), _)).Times(0);

    std::uint8_t buffer[MaxDownlinkFrameSize + 1];
    Beacon beacon(1s, buffer);
    const auto status = comm.SetBeacon(beacon);
    ASSERT_THAT(status, Eq(false));
}

TEST_F(CommTest, TestSetBeacon)
{
    const uint8_t data[] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8};
    Beacon beacon(0x0a0bs, data);
    EXPECT_CALL(i2c, WriteRead(TransmitterAddress, BeginsWith(TransmitterSendFrame), _))
        .WillOnce(Invoke([](uint8_t /*address*/, span<const uint8_t> inData, span<uint8_t> outData) {
            const uint8_t expected[] = {TransmitterSendFrame, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8};
            EXPECT_THAT(inData, Eq(span<const uint8_t>(expected)));

            outData[0] = 39;
            return I2CResult::OK;
        }));

    EXPECT_CALL(i2c, Write(TransmitterAddress, BeginsWith(TransmitterSetBeacon)))
        .WillOnce(Invoke([](uint8_t /*address*/, span<const uint8_t> inData) {
            const uint8_t expected[] = {TransmitterSetBeacon, 0x0b, 0x0a, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8};
            EXPECT_THAT(inData, Eq(span<const uint8_t>(expected)));
            return I2CResult::OK;
        }));
    const auto status = comm.SetBeacon(beacon);
    ASSERT_THAT(status, Eq(true));
}

TEST_F(CommTest, TestPauseNonExistingTask)
{
    OSMock system;
    auto guard = InstallProxy(&system);
    EXPECT_CALL(system, SuspendTask(_)).Times(0);
    const auto status = comm.Pause();
    ASSERT_THAT(status, Eq(true));
}

TEST_F(CommTest, TestTransmitterWatchdogResetFailure)
{
    i2c.ExpectWriteCommand(TransmitterAddress, TransmitterWatchdogReset).WillOnce(Return(I2CResult::Nack));
    ASSERT_THAT(comm.ResetWatchdogTransmitter(), Eq(false));
}

TEST_F(CommTest, TestTransmitterWatchdogReset)
{
    i2c.ExpectWriteCommand(TransmitterAddress, TransmitterWatchdogReset).WillOnce(Return(I2CResult::OK));
    ASSERT_THAT(comm.ResetWatchdogTransmitter(), Eq(true));
}

TEST_F(CommTest, TestReceiverWatchdogResetFailure)
{
    i2c.ExpectWriteCommand(ReceiverAddress, ReceiverWatchdogReset).WillOnce(Return(I2CResult::Nack));
    ASSERT_THAT(comm.ResetWatchdogReceiver(), Eq(false));
}

TEST_F(CommTest, TestReceiverWatchdogReset)
{
    i2c.ExpectWriteCommand(ReceiverAddress, ReceiverWatchdogReset).WillOnce(Return(I2CResult::OK));
    ASSERT_THAT(comm.ResetWatchdogReceiver(), Eq(true));
}

TEST_F(CommTest, TestPollHardwareQueriesFrameCountNoFrames)
{
    MockFrameCount(0);
    comm.PollHardware();
}

TEST_F(CommTest, TestPollHardwareProcessesValidFrames)
{
    std::uint8_t buffer[10] = {0};
    MockFrameCount(1);
    MockFrame(buffer, 1, 2);
    EXPECT_CALL(frameHandler, HandleFrame(_, _)).Times(1);
    comm.PollHardware();
}

TEST_F(CommTest, TestPollHardwareValidFramesAreRemoved)
{
    std::uint8_t buffer[10] = {0};
    MockFrameCount(1);
    MockFrame(buffer, 1, 2);
    MockRemoveFrame(I2CResult::OK);
    i2c.ExpectWriteCommand(ReceiverAddress, ReceiverWatchdogReset).WillOnce(Return(I2CResult::OK));
    comm.PollHardware();
}

TEST_F(CommTest, TestPollHardwareInValidFramesAreRemoved)
{
    std::uint8_t buffer[10] = {0};
    MockFrameCount(1);
    MockFrame(buffer, 0xffff, 0xffff);
    MockRemoveFrame(I2CResult::OK);
    i2c.ExpectWriteCommand(ReceiverAddress, ReceiverWatchdogReset).WillOnce(Return(I2CResult::OK));
    comm.PollHardware();
}

TEST_F(CommTest, TestPollHardwareRemoveFrameOnReceiveError)
{
    MockFrameCount(1);
    EXPECT_CALL(i2c, WriteRead(ReceiverAddress, ElementsAre(ReceiverGetFrame), _)).WillRepeatedly(Return(I2CResult::Nack));
    MockRemoveFrame(I2CResult::OK);
    i2c.ExpectWriteCommand(ReceiverAddress, ReceiverWatchdogReset).WillOnce(Return(I2CResult::OK));
    comm.PollHardware();
}

TEST_F(CommTest, TestPollHardwareDoNotTryToReceiveFrameOnQueryFailure)
{
    EXPECT_CALL(i2c, WriteRead(ReceiverAddress, ElementsAre(ReceiverGetFrameCount), _)).WillOnce(Return(I2CResult::Nack));

    comm.PollHardware();
}

TEST_F(CommTest, TestPollHardwareResetsWatchdogOnReceiver)
{
    EXPECT_CALL(i2c, WriteRead(ReceiverAddress, ElementsAre(ReceiverGetFrameCount), _)).WillOnce(Return(I2CResult::Nack));
    i2c.ExpectWriteCommand(ReceiverAddress, ReceiverWatchdogReset).WillOnce(Return(I2CResult::OK));
    comm.PollHardware();
}

TEST_F(CommTest, TestPollHardwareResetsWatchdogOnTransmitter)
{
    EXPECT_CALL(i2c, WriteRead(ReceiverAddress, ElementsAre(ReceiverGetFrameCount), _)).WillOnce(Return(I2CResult::Nack));
    i2c.ExpectWriteCommand(ReceiverAddress, ReceiverWatchdogReset).WillOnce(Return(I2CResult::Nack));
    i2c.ExpectWriteCommand(TransmitterAddress, TransmitterWatchdogReset).WillOnce(Return(I2CResult::OK));
    comm.PollHardware();
}

TEST_F(CommTest, TestRestartResetFailure)
{
    i2c.ExpectWriteCommand(ReceiverAddress, HardwareReset).WillOnce(Return(I2CResult::Nack));
    ASSERT_THAT(comm.Restart(), Eq(false));
}

TEST_F(CommTest, TestRestartTaskCreationFailure)
{
    i2c.ExpectWriteCommand(ReceiverAddress, HardwareReset).WillOnce(Return(I2CResult::OK));
    EXPECT_CALL(system, CreateTask(_, _, _, _, _, _)).WillOnce(Return(OSResult::IOError));
    ASSERT_THAT(comm.Restart(), Eq(false));
}

TEST_F(CommTest, TestRestart)
{
    i2c.ExpectWriteCommand(ReceiverAddress, HardwareReset).WillOnce(Return(I2CResult::OK));
    EXPECT_CALL(system, CreateTask(_, _, _, _, _, _)).WillOnce(Return(OSResult::Success));
    ASSERT_THAT(comm.Restart(), Eq(true));
}

TEST_F(CommTest, TestDoubleRestart)
{
    i2c.ExpectWriteCommand(ReceiverAddress, HardwareReset).WillOnce(Return(I2CResult::OK));
    EXPECT_CALL(system, CreateTask(_, _, _, _, _, _)).WillOnce(Invoke([](auto, auto, auto, auto, auto, auto handle) {
        *handle = reinterpret_cast<void*>(1);
        return OSResult::Success;
    }));

    ASSERT_THAT(comm.Restart(), Eq(true));
    ASSERT_THAT(comm.Restart(), Eq(true));
}

struct CommReceiverTelemetryTest : public testing::TestWithParam<std::tuple<int, uint8_t, I2CResult>>
{
    CommReceiverTelemetryTest();
    ErrorCountingConfigrationMock errorsConfig;
    error_counter::ErrorCounting errors;
    CommObject comm;
    FrameHandlerMock frameHandler;
    I2CBusMock i2c;
    testing::NiceMock<OSMock> system;
    OSReset reset;
};

CommReceiverTelemetryTest::CommReceiverTelemetryTest() : errors(errorsConfig), comm(errors, i2c, frameHandler)
{
    reset = SetupOs(comm, system);
}

TEST_P(CommReceiverTelemetryTest, TestInvalidTelemetry)
{
    ReceiverTelemetry telemetry;
    const auto index = std::get<0>(GetParam());
    const auto value = std::get<1>(GetParam());
    const auto operationStatus = std::get<2>(GetParam());

    UNUSED(index, value, operationStatus);

    EXPECT_CALL(i2c, WriteRead(ReceiverAddress, ElementsAre(ReceverGetTelemetry), _))
        .WillOnce(Invoke([&](uint8_t /*address*/, span<const uint8_t> /*inData*/, span<uint8_t> outData) {
            std::fill(outData.begin(), outData.end(), 0);
            outData[index] = value;
            return operationStatus;
        }));
    const auto status = comm.GetReceiverTelemetry(telemetry);
    ASSERT_THAT(status, Eq(false));
}

INSTANTIATE_TEST_CASE_P(CommReceiverTelemetryValuesOutOfRange,
    CommReceiverTelemetryTest,
    testing::Values(std::make_tuple(0, 0, I2CResult::Nack),
        std::make_tuple(1, 0xf0, I2CResult::OK),
        std::make_tuple(3, 0xf0, I2CResult::OK),
        std::make_tuple(5, 0xf0, I2CResult::OK),
        std::make_tuple(7, 0xf0, I2CResult::OK),
        std::make_tuple(9, 0xf0, I2CResult::OK),
        std::make_tuple(11, 0xf0, I2CResult::OK),
        std::make_tuple(13, 0xf0, I2CResult::OK)), );

struct CommTransmitterTelemetryTest : public testing::TestWithParam<std::tuple<int, uint8_t, I2CResult>>
{
    CommTransmitterTelemetryTest();
    ErrorCountingConfigrationMock errorsConfig;
    error_counter::ErrorCounting errors;
    CommObject comm;
    FrameHandlerMock frameHandler;
    I2CBusMock i2c;
    testing::NiceMock<OSMock> system;
    OSReset reset;
};

CommTransmitterTelemetryTest::CommTransmitterTelemetryTest() : errors(errorsConfig), comm(errors, i2c, frameHandler)
{
    reset = SetupOs(comm, system);
}

TEST_P(CommTransmitterTelemetryTest, TestInvalidTelemetry)
{
    TransmitterTelemetry telemetry;
    const auto index = std::get<0>(GetParam());
    const auto value = std::get<1>(GetParam());
    const auto operationStatus = std::get<2>(GetParam());
    UNUSED(index, value, operationStatus);
    EXPECT_CALL(i2c, WriteRead(TransmitterAddress, ElementsAre(TransmitterGetTelemetry), _))
        .WillOnce(Invoke([=](uint8_t /*address*/, span<const uint8_t> /*inData*/, span<uint8_t> outData) {
            std::fill(outData.begin(), outData.end(), 0);
            outData[index] = value;
            return operationStatus;
        }));
    const auto status = comm.GetTransmitterTelemetry(telemetry);
    ASSERT_THAT(status, Eq(false));
}

INSTANTIATE_TEST_CASE_P(CommTransmitterTelemetryValuesOutOfRange,
    CommTransmitterTelemetryTest,
    testing::Values(std::make_tuple(0, 0, I2CResult::Nack),
        std::make_tuple(1, 0xf0, I2CResult::OK),
        std::make_tuple(3, 0xf0, I2CResult::OK),
        std::make_tuple(5, 0xf0, I2CResult::OK),
        std::make_tuple(7, 0xf0, I2CResult::OK)), );
