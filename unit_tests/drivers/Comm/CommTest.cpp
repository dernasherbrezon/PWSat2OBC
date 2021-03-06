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
#include "comm/CommTelemetry.hpp"
#include "comm/Frame.hpp"
#include "comm/IHandleFrame.hpp"
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
using testing::InSequence;
using testing::ReturnPointee;
using testing::DoAll;
using testing::Assign;
using gsl::span;
using drivers::i2c::I2CResult;
using namespace devices::comm;
using namespace std::chrono_literals;

namespace
{
    static constexpr uint8_t ReceiverAddress = 0x60;
    static constexpr uint8_t TransmitterAddress = 0x61;

    static constexpr uint8_t ReceiverGetTelemetry = 0x1A;
    static constexpr uint8_t ReceiverGetFrameCount = 0x21;
    static constexpr uint8_t ReceiverGetFrame = 0x22;
    static constexpr uint8_t ReceiverRemoveFrame = 0x24;
    static constexpr uint8_t ReceiverReset = 0xAA;
    static constexpr uint8_t ReceiverWatchdogReset = 0xCC;
    static constexpr uint8_t ReceiverGetUptime = 0x40;

    static constexpr uint8_t HardwareReset = 0xAB;

    static constexpr uint8_t TransmitterSendFrame = 0x10;
    static constexpr uint8_t TransmitterSetBeacon = 0x14;
    static constexpr uint8_t TransmitterClearBeacon = 0x1f;
    static constexpr uint8_t TransmitterSetIdleState = 0x24;
    static constexpr uint8_t TransmitterGetTelemetryLastTransmission = 0x26;
    static constexpr uint8_t TransmitterGetTelemetryInstant = 0x25;
    static constexpr uint8_t TransmitterSetBitrate = 0x28;
    static constexpr uint8_t TransmitterGetState = 0x41;
    static constexpr uint8_t TransmitterGetUptime = 0x40;
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
        MOCK_METHOD2(HandleFrame, void(ITransmitter&, Frame&));
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
        testing::NiceMock<ErrorCountingConfigrationMock> errorsConfig;
        error_counter::ErrorCounting errors;
        error_counter::ErrorCounter<0> error_counter;

        void MockFrameCount(std::uint16_t frameCount);

        void ExpectSendFrame(gsl::span<const std::uint8_t> expected, std::uint8_t freeSlotCount);

        void ExpectSendFrame(std::uint8_t freeSlotCount);
        void ExpectSendFrame(std::uint8_t freeSlotCount, std::function<void()> after);

        void MockFrame(gsl::span<const std::uint8_t> span, std::uint16_t rssi = 0, std::uint16_t doppler = 0);

        void MockRemoveFrame(I2CResult result);
    };

    CommTest::CommTest() : comm(errors, i2c), errors(errorsConfig), error_counter(errors)
    {
        reset = SetupOs(comm, system);
        comm.SetFrameHandler(frameHandler);
    }

    void CommTest::MockFrameCount(std::uint16_t frameCount)
    {
        EXPECT_CALL(i2c, Write(ReceiverAddress, BeginsWith(ReceiverGetFrameCount)))
            .WillRepeatedly(Invoke([=](std::uint8_t /*address*/, auto /*data*/) {
                EXPECT_CALL(i2c, Read(ReceiverAddress, _)).WillOnce(Invoke([=](uint8_t /*address*/, auto outData) {
                    outData[0] = frameCount & 0xff;
                    outData[1] = (frameCount >> 8) & 0xff;
                    return I2CResult::OK;
                }));
                return I2CResult::OK;
            }));
    }

    void CommTest::MockFrame(gsl::span<const std::uint8_t> data, std::uint16_t rssi, std::uint16_t doppler)
    {
        EXPECT_CALL(i2c, Write(ReceiverAddress, ElementsAre(ReceiverGetFrame)))
            .WillRepeatedly(Invoke([=](std::uint8_t /*address*/, auto /*data*/) {
                EXPECT_CALL(i2c, Read(ReceiverAddress, _)).WillOnce(Invoke([=](uint8_t /*address*/, gsl::span<uint8_t> outData) {
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

                return I2CResult::OK;
            }));
    }

    void CommTest::ExpectSendFrame(gsl::span<const std::uint8_t> expected, std::uint8_t freeSlotCount)
    {
        EXPECT_CALL(i2c, Write(TransmitterAddress, BeginsWith(TransmitterSendFrame)))
            .WillOnce(Invoke([=](uint8_t /*address*/, span<const uint8_t> inData) {
                EXPECT_THAT(inData, Eq(expected));
                EXPECT_CALL(i2c, Read(TransmitterAddress, _)).WillOnce(Invoke([=](uint8_t /*address*/, span<uint8_t> outData) {
                    outData[0] = freeSlotCount;
                    return I2CResult::OK;
                }));
                return I2CResult::OK;
            }));
    }

    void CommTest::ExpectSendFrame(std::uint8_t freeSlotCount)
    {
        EXPECT_CALL(i2c, Write(TransmitterAddress, BeginsWith(TransmitterSendFrame)))
            .WillOnce(Invoke([=](uint8_t /*address*/, span<const uint8_t> /*inData*/) {
                EXPECT_CALL(i2c, Read(TransmitterAddress, _)).WillOnce(Invoke([=](uint8_t /*address*/, span<uint8_t> outData) {
                    outData[0] = freeSlotCount;
                    return I2CResult::OK;
                }));
                return I2CResult::OK;
            }));
    }

    void CommTest::ExpectSendFrame(std::uint8_t freeSlotCount, std::function<void()> after)
    {
        EXPECT_CALL(i2c, Write(TransmitterAddress, BeginsWith(TransmitterSendFrame)))
            .WillOnce(Invoke([=](uint8_t /*address*/, span<const uint8_t> /*inData*/) {
                EXPECT_CALL(i2c, Read(TransmitterAddress, _)).WillOnce(Invoke([=](uint8_t /*address*/, span<uint8_t> outData) {
                    outData[0] = freeSlotCount;

                    after();

                    return I2CResult::OK;
                }));
                return I2CResult::OK;
            }));
    }

    void CommTest::MockRemoveFrame(I2CResult result)
    {
        i2c.ExpectWriteCommand(ReceiverAddress, ReceiverRemoveFrame).WillOnce(Return(result));
    }

    TEST_F(CommTest, TestInitializationDoesNotTouchHardware)
    {
        CommObject commObject(errors, i2c);

        EXPECT_CALL(i2c, WriteRead(_, _, _)).Times(0);
        EXPECT_CALL(i2c, Write(_, _)).Times(0);

        commObject.Initialize();
    }

    TEST_F(CommTest, TestInitializationEventGroupAllocationFailure)
    {
        CommObject commObject(errors, i2c);

        EXPECT_CALL(system, CreateEventGroup()).WillOnce(Return(nullptr));

        const auto status = commObject.Initialize();

        ASSERT_THAT(status, Ne(OSResult::Success));
        ASSERT_THAT(error_counter, Eq(5));
    }

    TEST_F(CommTest, TestInitializationTaskCreationFailure)
    {
        CommObject commObject(errors, i2c);

        EXPECT_CALL(system, CreateTask(_, _, _, _, _, _)).WillOnce(Return(OSResult::AccessDenied));

        const auto status = commObject.Initialize();

        ASSERT_THAT(status, Ne(OSResult::Success));
    }

    TEST_F(CommTest, TestInitialization)
    {
        CommObject commObject(errors, i2c);

        EXPECT_CALL(system, SuspendTask(_));

        const auto status = commObject.Initialize();

        ASSERT_THAT(status, Eq(OSResult::Success));
        ASSERT_THAT(error_counter, Eq(0));
    }

    TEST_F(CommTest, TestHardwareReset)
    {
        i2c.ExpectWriteCommand(ReceiverAddress, HardwareReset).WillOnce(Return(I2CResult::OK));
        const auto status = comm.Reset();
        ASSERT_THAT(status, Eq(true));
        ASSERT_THAT(error_counter, Eq(0));
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
        ASSERT_THAT(error_counter, Eq(0));
    }

    TEST_F(CommTest, TestTransmitterResetFailure)
    {
        i2c.ExpectWriteCommand(TransmitterAddress, TransmitterReset).WillOnce(Return(I2CResult::Nack));
        const auto status = comm.ResetTransmitter();
        ASSERT_THAT(status, Eq(false));
        ASSERT_THAT(error_counter, Eq(5));
    }

    TEST_F(CommTest, TestReceiverReset)
    {
        i2c.ExpectWriteCommand(ReceiverAddress, ReceiverReset).WillOnce(Return(I2CResult::OK));
        const auto status = comm.ResetReceiver();
        ASSERT_THAT(status, Eq(true));
        ASSERT_THAT(error_counter, Eq(0));
    }

    TEST_F(CommTest, TestReceiverResetFailure)
    {
        i2c.ExpectWriteCommand(ReceiverAddress, ReceiverReset).WillOnce(Return(I2CResult::Nack));
        const auto status = comm.ResetReceiver();
        ASSERT_THAT(status, Eq(false));
        ASSERT_THAT(error_counter, Eq(5));
    }

    TEST_F(CommTest, TestFrameRemoval)
    {
        MockRemoveFrame(I2CResult::OK);
        const auto status = comm.RemoveFrame();
        ASSERT_THAT(status, Eq(true));
        ASSERT_THAT(error_counter, Eq(0));
    }

    TEST_F(CommTest, TestFrameRemovalFailure)
    {
        MockRemoveFrame(I2CResult::Nack);
        const auto status = comm.RemoveFrame();
        ASSERT_THAT(status, Eq(false));
        ASSERT_THAT(error_counter, Eq(5));
    }

    TEST_F(CommTest, TestGetFrameRequestFailure)
    {
        i2c.ExpectWriteCommand(ReceiverAddress, ReceiverGetFrameCount).WillOnce(Return(I2CResult::Nack));
        const auto result = comm.GetFrameCount();
        ASSERT_THAT(result.status, Eq(false));
        ASSERT_THAT(result.frameCount, Eq(0));
        ASSERT_THAT(error_counter, Eq(5));
    }

    TEST_F(CommTest, TestGetFrameResponseFailure)
    {
        i2c.ExpectWriteCommand(ReceiverAddress, ReceiverGetFrameCount).WillOnce(Return(I2CResult::OK));
        EXPECT_CALL(i2c, Read(ReceiverAddress, _)).WillOnce(Return(I2CResult::Nack));
        const auto result = comm.GetFrameCount();
        ASSERT_THAT(result.status, Eq(false));
        ASSERT_THAT(result.frameCount, Eq(0));
        ASSERT_THAT(error_counter, Eq(5));
    }

    TEST_F(CommTest, TestGetFrameCountOnLimit)
    {
        MockFrameCount(64);
        const auto result = comm.GetFrameCount();
        ASSERT_THAT(result.status, Eq(true));
        ASSERT_THAT(result.frameCount, Eq(64));
        ASSERT_THAT(error_counter, Eq(0));
    }

    TEST_F(CommTest, TestGetFrameCountAboveLimit)
    {
        MockFrameCount(65);
        const auto result = comm.GetFrameCount();
        ASSERT_THAT(result.status, Eq(false));
        ASSERT_THAT(result.frameCount, Eq(0));
    }

    TEST_F(CommTest, TestClearBeaconFailure)
    {
        i2c.ExpectWriteCommand(TransmitterAddress, TransmitterClearBeacon).WillOnce(Return(I2CResult::Nack));
        const auto status = comm.ClearBeacon();
        ASSERT_THAT(status, Eq(false));
        ASSERT_THAT(error_counter, Eq(5));
    }

    TEST_F(CommTest, TestClearBeacon)
    {
        i2c.ExpectWriteCommand(TransmitterAddress, TransmitterClearBeacon).WillOnce(Return(I2CResult::OK));
        const auto status = comm.ClearBeacon();
        ASSERT_THAT(status, Eq(true));
        ASSERT_THAT(error_counter, Eq(0));
    }

    TEST_F(CommTest, TestSetIdleStateFailure)
    {
        EXPECT_CALL(i2c, Write(TransmitterAddress, ElementsAre(TransmitterSetIdleState, _))).WillOnce(Return(I2CResult::Nack));
        const auto status = comm.SetTransmitterStateWhenIdle(IdleState::On);
        ASSERT_THAT(status, Eq(false));
        ASSERT_THAT(error_counter, Eq(5));
    }

    TEST_F(CommTest, TestSetIdleState)
    {
        EXPECT_CALL(i2c, Write(TransmitterAddress, ElementsAre(TransmitterSetIdleState, _))).WillOnce(Return(I2CResult::OK));
        const auto status = comm.SetTransmitterStateWhenIdle(IdleState::On);
        ASSERT_THAT(status, Eq(true));
        ASSERT_THAT(error_counter, Eq(0));
    }

    TEST_F(CommTest, TestSetIdleStateOn)
    {
        EXPECT_CALL(i2c, Write(TransmitterAddress, ElementsAre(TransmitterSetIdleState, _)))
            .WillOnce(Invoke([](uint8_t /*address*/, auto inData) {
                EXPECT_THAT(inData[1], Eq(1));
                return I2CResult::OK;
            }));
        const auto status = comm.SetTransmitterStateWhenIdle(IdleState::On);
        ASSERT_THAT(status, Eq(true));
        ASSERT_THAT(error_counter, Eq(0));
    }

    TEST_F(CommTest, TestSetIdleStateOff)
    {
        EXPECT_CALL(i2c, Write(TransmitterAddress, ElementsAre(TransmitterSetIdleState, _)))
            .WillOnce(Invoke([](uint8_t /*address*/, auto inData) {
                EXPECT_THAT(inData[1], Eq(0));
                return I2CResult::OK;
            }));
        const auto status = comm.SetTransmitterStateWhenIdle(IdleState::Off);
        ASSERT_THAT(status, Eq(true));
        ASSERT_THAT(error_counter, Eq(0));
    }

    TEST_F(CommTest, TestSetBitrateFailure)
    {
        EXPECT_CALL(i2c, Write(TransmitterAddress, ElementsAre(TransmitterSetBitrate, _))).WillOnce(Return(I2CResult::Nack));
        const auto status = comm.SetTransmitterBitRate(Bitrate::Comm1200bps);
        ASSERT_THAT(status, Eq(false));
        ASSERT_THAT(error_counter, Eq(5));
    }

    TEST_F(CommTest, TestSetBitrate)
    {
        EXPECT_CALL(i2c, Write(TransmitterAddress, ElementsAre(TransmitterSetBitrate, _))).WillOnce(Return(I2CResult::OK));
        const auto status = comm.SetTransmitterBitRate(Bitrate::Comm1200bps);
        ASSERT_THAT(status, Eq(true));
        ASSERT_THAT(error_counter, Eq(0));
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
        ASSERT_THAT(error_counter, Eq(0));
    }

    TEST_F(CommTest, TestSendTooLongFrame)
    {
        uint8_t buffer[devices::comm::MaxDownlinkFrameSize + 1] = {0};
        const auto status = comm.SendFrame(buffer);
        ASSERT_THAT(status, Eq(false));
        ASSERT_THAT(error_counter, Eq(0));
    }

    TEST_F(CommTest, TestSendFrameRequestFailure)
    {
        uint8_t buffer[10] = {0};
        EXPECT_CALL(i2c, Write(TransmitterAddress, BeginsWith(TransmitterSendFrame))).WillOnce(Return(I2CResult::Nack));
        const auto status = comm.SendFrame(span<const uint8_t, 10>(buffer));
        ASSERT_THAT(status, Eq(false));
        ASSERT_THAT(error_counter, Eq(5));
    }

    TEST_F(CommTest, TestSendFrameResponseFailure)
    {
        uint8_t buffer[10] = {0};
        EXPECT_CALL(i2c, Write(TransmitterAddress, BeginsWith(TransmitterSendFrame))).WillOnce(Return(I2CResult::OK));
        EXPECT_CALL(i2c, Read(TransmitterAddress, _)).WillOnce(Return(I2CResult::Nack));
        const auto status = comm.SendFrame(span<const uint8_t, 10>(buffer));
        ASSERT_THAT(status, Eq(false));
        ASSERT_THAT(error_counter, Eq(5));
    }

    TEST_F(CommTest, TestSendFrame)
    {
        uint8_t buffer[] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc};
        const uint8_t expected[] = {TransmitterSendFrame, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc};
        ExpectSendFrame(gsl::make_span(expected), 0);
        const auto status = comm.SendFrame(span<const uint8_t>(buffer));
        ASSERT_THAT(status, Eq(true));
        ASSERT_THAT(error_counter, Eq(0));
    }

    TEST_F(CommTest, TestSendFrameRejectedByHardware)
    {
        uint8_t buffer[] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc};
        ExpectSendFrame(0xff);
        const auto status = comm.SendFrame(span<const uint8_t>(buffer));
        ASSERT_THAT(status, Eq(false));
        ASSERT_THAT(error_counter, Eq(5));
    }

    TEST_F(CommTest, TestReceiveFrameRequestFailure)
    {
        Frame frame;
        EXPECT_CALL(i2c, Write(ReceiverAddress, ElementsAre(ReceiverGetFrame))).WillOnce(Return(I2CResult::Nack));
        const auto status = comm.ReceiveFrame(dataBuffer, frame);
        ASSERT_THAT(status, Eq(false));
        ASSERT_THAT(error_counter, Eq(5));
    }

    TEST_F(CommTest, TestReceiveFrameResponseFailure)
    {
        Frame frame;
        EXPECT_CALL(i2c, Write(ReceiverAddress, ElementsAre(ReceiverGetFrame))).WillOnce(Return(I2CResult::OK));
        EXPECT_CALL(i2c, Read(ReceiverAddress, _)).WillOnce(Return(I2CResult::Nack));
        const auto status = comm.ReceiveFrame(dataBuffer, frame);
        ASSERT_THAT(status, Eq(false));
        ASSERT_THAT(error_counter, Eq(5));
    }

    TEST_F(CommTest, TestReceiveFrameTooShortBuffer)
    {
        std::uint8_t buffer[1];
        Frame frame;
        EXPECT_CALL(i2c, Write(ReceiverAddress, ElementsAre(ReceiverGetFrame))).Times(0);
        EXPECT_CALL(i2c, Read(ReceiverAddress, _)).Times(0);
        const auto status = comm.ReceiveFrame(buffer, frame);
        ASSERT_THAT(status, Eq(false));
        ASSERT_THAT(error_counter, Eq(0));
    }

    TEST_F(CommTest, TestReceiveFramePartialHeader)
    {
        std::uint8_t buffer[5];
        Frame frame;
        EXPECT_CALL(i2c, Read(ReceiverAddress, _)).WillRepeatedly(Invoke([&](uint8_t /*address*/, span<uint8_t> outData) {
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
        ASSERT_THAT(error_counter, Eq(0));
    }

    TEST_F(CommTest, TestReceiveFrameReceiveFailure)
    {
        Frame frame;
        EXPECT_CALL(i2c, Read(ReceiverAddress, _))
            .WillOnce(Invoke([](uint8_t /*address*/, span<uint8_t> outData) {
                std::fill(outData.begin(), outData.end(), 0);
                outData[0] = 1;
                return I2CResult::OK;
            }))
            .WillOnce(Return(I2CResult::Nack));
        const auto status = comm.ReceiveFrame(dataBuffer, frame);
        ASSERT_THAT(status, Eq(false));
        ASSERT_THAT(error_counter, Eq(5));
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
        ASSERT_THAT(error_counter, Eq(0));
    }

    TEST_F(CommTest, TestReceiveFramePartialData)
    {
        std::uint8_t buffer[22];
        Frame frame;
        const uint8_t expected[] = {0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
        EXPECT_CALL(i2c, Read(ReceiverAddress, _)).WillRepeatedly(Invoke([&](uint8_t /*address*/, span<uint8_t> outData) {
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
        ASSERT_THAT(error_counter, Eq(0));
    }

    TEST_F(CommTest, TestReceiverTelemetry)
    {
        ReceiverTelemetry telemetry;

        uint8_t uptimeRaw[] = {5, 20, 3, 1};
        uint8_t frame[] = {0x05, 0x00, 0x03, 0x04, 0x05, 0x06, 1, 2, 3, 4, 5};

        uint8_t nowRaw[] = {0x00, 0x00, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0e, 0x0d, 0xce, 0x0e};

        gsl::span<std::uint8_t> i2cResponse;

        EXPECT_CALL(i2c, Write(ReceiverAddress, ElementsAre(ReceiverGetFrame)))
            .Times(2)
            .WillOnce(DoAll(Assign(&i2cResponse, gsl::make_span(frame)), Return(I2CResult::OK)));

        EXPECT_CALL(i2c, Write(ReceiverAddress, ElementsAre(ReceiverGetUptime)))
            .WillOnce(DoAll(Assign(&i2cResponse, gsl::make_span(uptimeRaw)), Return(I2CResult::OK)));

        EXPECT_CALL(i2c, Write(ReceiverAddress, ElementsAre(ReceiverGetTelemetry)))
            .WillOnce(DoAll(Assign(&i2cResponse, gsl::make_span(nowRaw)), Return(I2CResult::OK)));

        ON_CALL(i2c, Read(ReceiverAddress, _)).WillByDefault(Invoke([&](uint8_t /*address*/, span<uint8_t> outData) {
            std::fill(outData.begin(), outData.end(), 0);
            std::copy_n(std::begin(i2cResponse), std::min<std::size_t>(i2cResponse.size(), outData.size()), outData.begin());
            return I2CResult::OK;
        }));

        std::array<std::uint8_t, 250> frameBuf;
        Frame f;
        auto s = comm.ReceiveFrame(frameBuf, f);

        ASSERT_THAT(s, Eq(true));

        ASSERT_THAT(f.Doppler(), Eq(0x0403U));

        const auto status = comm.GetReceiverTelemetry(telemetry);

        ASSERT_THAT(telemetry.Uptime, Eq(27h + 20min + 5s));
        ASSERT_THAT(telemetry.LastReceivedDopplerOffset, Eq(0x0403));
        ASSERT_THAT(telemetry.LastReceivedRSSI, Eq(0x0605));
        ASSERT_THAT(telemetry.NowDopplerOffset, Eq(0x0807));
        ASSERT_THAT(telemetry.NowReceiverCurrentConsumption, Eq(0x0a09));
        ASSERT_THAT(telemetry.NowVoltage, Eq(0x0c0b));
        ASSERT_THAT(telemetry.NowOscilatorTemperature, Eq(0x0e0d));
        ASSERT_THAT(telemetry.NowAmplifierTemperature, Eq(0x0d0e));
        ASSERT_THAT(telemetry.NowRSSI, Eq(0x0ece));

        ASSERT_THAT(status, Eq(true));
        ASSERT_THAT(error_counter, Eq(0));
    }

    TEST_F(CommTest, TestReceiverTelemetryFailureGetUptime)
    {
        ReceiverTelemetry telemetry;

        uint8_t frame[] = {0x05, 0x00, 0x03, 0x04, 0x05, 0x06, 1, 2, 3, 4, 5};

        uint8_t nowRaw[] = {0x00, 0x00, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0e, 0x0d, 0xce, 0x0e};

        gsl::span<std::uint8_t> i2cResponse;

        EXPECT_CALL(i2c, Write(ReceiverAddress, ElementsAre(ReceiverGetFrame)))
            .Times(2)
            .WillOnce(DoAll(Assign(&i2cResponse, gsl::make_span(frame)), Return(I2CResult::OK)));

        EXPECT_CALL(i2c, Write(ReceiverAddress, ElementsAre(ReceiverGetUptime))).WillOnce(Return(I2CResult::Nack));

        EXPECT_CALL(i2c, Write(ReceiverAddress, ElementsAre(ReceiverGetTelemetry)))
            .WillOnce(DoAll(Assign(&i2cResponse, gsl::make_span(nowRaw)), Return(I2CResult::OK)));

        ON_CALL(i2c, Read(ReceiverAddress, _)).WillByDefault(Invoke([&](uint8_t /*address*/, span<uint8_t> outData) {
            std::fill(outData.begin(), outData.end(), 0);
            std::copy_n(std::begin(i2cResponse), std::min<std::size_t>(i2cResponse.size(), outData.size()), outData.begin());
            return I2CResult::OK;
        }));

        std::array<std::uint8_t, 250> frameBuf;
        Frame f;
        auto s = comm.ReceiveFrame(frameBuf, f);

        ASSERT_THAT(s, Eq(true));

        ASSERT_THAT(f.Doppler(), Eq(0x0403U));

        const auto status = comm.GetReceiverTelemetry(telemetry);

        ASSERT_THAT(telemetry.Uptime, Eq(0s));
        ASSERT_THAT(telemetry.LastReceivedDopplerOffset, Eq(0x0403));
        ASSERT_THAT(telemetry.LastReceivedRSSI, Eq(0x0605));
        ASSERT_THAT(telemetry.NowDopplerOffset, Eq(0x0807));
        ASSERT_THAT(telemetry.NowReceiverCurrentConsumption, Eq(0x0a09));
        ASSERT_THAT(telemetry.NowVoltage, Eq(0x0c0b));
        ASSERT_THAT(telemetry.NowOscilatorTemperature, Eq(0x0e0d));
        ASSERT_THAT(telemetry.NowAmplifierTemperature, Eq(0x0d0e));
        ASSERT_THAT(telemetry.NowRSSI, Eq(0x0ece));

        ASSERT_THAT(status, Eq(false));
        ASSERT_THAT(error_counter, Eq(5));
    }

    TEST_F(CommTest, TestReceiverTelemetryFailureGetTelemetryInstant)
    {
        ReceiverTelemetry telemetry;

        uint8_t uptimeRaw[] = {5, 20, 3, 1};
        uint8_t frame[] = {0x05, 0x00, 0x03, 0x04, 0x05, 0x06, 1, 2, 3, 4, 5};

        gsl::span<std::uint8_t> i2cResponse;

        EXPECT_CALL(i2c, Write(ReceiverAddress, ElementsAre(ReceiverGetFrame)))
            .Times(2)
            .WillOnce(DoAll(Assign(&i2cResponse, gsl::make_span(frame)), Return(I2CResult::OK)));

        EXPECT_CALL(i2c, Write(ReceiverAddress, ElementsAre(ReceiverGetUptime)))
            .WillOnce(DoAll(Assign(&i2cResponse, gsl::make_span(uptimeRaw)), Return(I2CResult::OK)));

        EXPECT_CALL(i2c, Write(ReceiverAddress, ElementsAre(ReceiverGetTelemetry))).WillOnce(Return(I2CResult::Nack));

        ON_CALL(i2c, Read(ReceiverAddress, _)).WillByDefault(Invoke([&](uint8_t /*address*/, span<uint8_t> outData) {
            std::fill(outData.begin(), outData.end(), 0);
            std::copy_n(std::begin(i2cResponse), std::min<std::size_t>(i2cResponse.size(), outData.size()), outData.begin());
            return I2CResult::OK;
        }));

        std::array<std::uint8_t, 250> frameBuf;
        Frame f;
        auto s = comm.ReceiveFrame(frameBuf, f);

        ASSERT_THAT(s, Eq(true));

        ASSERT_THAT(f.Doppler(), Eq(0x0403U));

        const auto status = comm.GetReceiverTelemetry(telemetry);

        ASSERT_THAT(telemetry.Uptime, Eq(27h + 20min + 5s));
        ASSERT_THAT(telemetry.LastReceivedDopplerOffset, Eq(0x0403));
        ASSERT_THAT(telemetry.LastReceivedRSSI, Eq(0x0605));
        ASSERT_THAT(telemetry.NowDopplerOffset, Eq(0));
        ASSERT_THAT(telemetry.NowReceiverCurrentConsumption, Eq(0));
        ASSERT_THAT(telemetry.NowVoltage, Eq(0));
        ASSERT_THAT(telemetry.NowOscilatorTemperature, Eq(0));
        ASSERT_THAT(telemetry.NowAmplifierTemperature, Eq(0));
        ASSERT_THAT(telemetry.NowRSSI, Eq(0));

        ASSERT_THAT(status, Eq(false));
        ASSERT_THAT(error_counter, Eq(5));
    }

    TEST_F(CommTest, TestTransmitterTelemetry)
    {
        TransmitterTelemetry telemetry;

        uint8_t uptimeRaw[] = {5, 20, 3, 1};
        uint8_t stateRaw[] = {0b00001011};
        uint8_t lastTransmissionRaw[] = {0x21, 0x04, 0xB0, 0x04, 0x41, 0x05, 0x82, 0x05};
        uint8_t nowRaw[] = {0x00, 0x00, 0x11, 0x11, 0x49, 0x0C, 0xAA, 0x0F};

        gsl::span<std::uint8_t> i2cResponse;

        EXPECT_CALL(i2c, Write(TransmitterAddress, ElementsAre(TransmitterGetUptime)))
            .WillOnce(DoAll(Assign(&i2cResponse, gsl::make_span(uptimeRaw)), Return(I2CResult::OK)));

        EXPECT_CALL(i2c, Write(TransmitterAddress, ElementsAre(TransmitterGetState)))
            .WillOnce(DoAll(Assign(&i2cResponse, gsl::make_span(stateRaw)), Return(I2CResult::OK)));

        EXPECT_CALL(i2c, Write(TransmitterAddress, ElementsAre(TransmitterGetTelemetryLastTransmission)))
            .WillOnce(DoAll(Assign(&i2cResponse, gsl::make_span(lastTransmissionRaw)), Return(I2CResult::OK)));

        EXPECT_CALL(i2c, Write(TransmitterAddress, ElementsAre(TransmitterGetTelemetryInstant)))
            .WillOnce(DoAll(Assign(&i2cResponse, gsl::make_span(nowRaw)), Return(I2CResult::OK)));

        ON_CALL(i2c, Read(TransmitterAddress, _)).WillByDefault(Invoke([&](uint8_t /*address*/, span<uint8_t> outData) {
            std::fill(outData.begin(), outData.end(), 0);
            std::copy(std::begin(i2cResponse), std::end(i2cResponse), outData.begin());
            return I2CResult::OK;
        }));

        const auto status = comm.GetTransmitterTelemetry(telemetry);
        ASSERT_THAT(telemetry.Uptime, Eq(27h + 20min + 5s));
        ASSERT_THAT(telemetry.TransmitterBitRate, Eq(Bitrate::Comm4800bps));

        ASSERT_THAT(telemetry.LastTransmittedRFReflectedPower, Eq(1057));
        ASSERT_THAT(telemetry.LastTransmittedAmplifierTemperature, Eq(1200));
        ASSERT_THAT(telemetry.LastTransmittedRFForwardPower, Eq(1345));
        ASSERT_THAT(telemetry.LastTransmittedTransmitterCurrentConsumption, Eq(1410));

        ASSERT_THAT(telemetry.NowRFForwardPower, Eq(3145));
        ASSERT_THAT(telemetry.NowTransmitterCurrentConsumption, Eq(4010));

        ASSERT_THAT(telemetry.StateWhenIdle, Eq(IdleState::On));
        ASSERT_THAT(telemetry.BeaconState, Eq(true));

        ASSERT_THAT(status, Eq(true));
        ASSERT_THAT(error_counter, Eq(0));
    }

    TEST_F(CommTest, TestGetTelemetryTransmitterFailureGetUptime)
    {
        TransmitterTelemetry telemetry;

        uint8_t stateRaw[] = {0b00001011};
        uint8_t lastTransmissionRaw[] = {0x21, 0x04, 0xB0, 0x04, 0x41, 0x05, 0x82, 0x05};
        uint8_t nowRaw[] = {0x00, 0x00, 0x11, 0x11, 0x49, 0x0C, 0xAA, 0x0F};

        gsl::span<std::uint8_t> i2cResponse;

        EXPECT_CALL(i2c, Write(TransmitterAddress, ElementsAre(TransmitterGetUptime))).WillOnce(Return(I2CResult::Nack));

        EXPECT_CALL(i2c, Write(TransmitterAddress, ElementsAre(TransmitterGetState)))
            .WillOnce(DoAll(Assign(&i2cResponse, gsl::make_span(stateRaw)), Return(I2CResult::OK)));

        EXPECT_CALL(i2c, Write(TransmitterAddress, ElementsAre(TransmitterGetTelemetryLastTransmission)))
            .WillOnce(DoAll(Assign(&i2cResponse, gsl::make_span(lastTransmissionRaw)), Return(I2CResult::OK)));

        EXPECT_CALL(i2c, Write(TransmitterAddress, ElementsAre(TransmitterGetTelemetryInstant)))
            .WillOnce(DoAll(Assign(&i2cResponse, gsl::make_span(nowRaw)), Return(I2CResult::OK)));

        ON_CALL(i2c, Read(TransmitterAddress, _)).WillByDefault(Invoke([&](uint8_t /*address*/, span<uint8_t> outData) {
            std::fill(outData.begin(), outData.end(), 0);
            std::copy(std::begin(i2cResponse), std::end(i2cResponse), outData.begin());
            return I2CResult::OK;
        }));

        const auto status = comm.GetTransmitterTelemetry(telemetry);
        ASSERT_THAT(telemetry.Uptime, Eq(0s));
        ASSERT_THAT(telemetry.TransmitterBitRate, Eq(Bitrate::Comm4800bps));

        ASSERT_THAT(telemetry.LastTransmittedRFReflectedPower, Eq(1057));
        ASSERT_THAT(telemetry.LastTransmittedAmplifierTemperature, Eq(1200));
        ASSERT_THAT(telemetry.LastTransmittedRFForwardPower, Eq(1345));
        ASSERT_THAT(telemetry.LastTransmittedTransmitterCurrentConsumption, Eq(1410));

        ASSERT_THAT(telemetry.NowRFForwardPower, Eq(3145));
        ASSERT_THAT(telemetry.NowTransmitterCurrentConsumption, Eq(4010));

        ASSERT_THAT(telemetry.StateWhenIdle, Eq(IdleState::On));
        ASSERT_THAT(telemetry.BeaconState, Eq(true));

        ASSERT_THAT(status, Eq(false));
        ASSERT_THAT(error_counter, Eq(5));
    }

    TEST_F(CommTest, TestTransmitterTelemetryFailureGetState)
    {
        TransmitterTelemetry telemetry;

        uint8_t uptimeRaw[] = {5, 20, 3, 1};
        uint8_t lastTransmissionRaw[] = {0x21, 0x04, 0xB0, 0x04, 0x41, 0x05, 0x82, 0x05};
        uint8_t nowRaw[] = {0x00, 0x00, 0x11, 0x11, 0x49, 0x0C, 0xAA, 0x0F};

        gsl::span<std::uint8_t> i2cResponse;

        EXPECT_CALL(i2c, Write(TransmitterAddress, ElementsAre(TransmitterGetUptime)))
            .WillOnce(DoAll(Assign(&i2cResponse, gsl::make_span(uptimeRaw)), Return(I2CResult::OK)));

        EXPECT_CALL(i2c, Write(TransmitterAddress, ElementsAre(TransmitterGetState))).WillOnce(Return(I2CResult::Nack));

        EXPECT_CALL(i2c, Write(TransmitterAddress, ElementsAre(TransmitterGetTelemetryLastTransmission)))
            .WillOnce(DoAll(Assign(&i2cResponse, gsl::make_span(lastTransmissionRaw)), Return(I2CResult::OK)));

        EXPECT_CALL(i2c, Write(TransmitterAddress, ElementsAre(TransmitterGetTelemetryInstant)))
            .WillOnce(DoAll(Assign(&i2cResponse, gsl::make_span(nowRaw)), Return(I2CResult::OK)));

        ON_CALL(i2c, Read(TransmitterAddress, _)).WillByDefault(Invoke([&](uint8_t /*address*/, span<uint8_t> outData) {
            std::fill(outData.begin(), outData.end(), 0);
            std::copy(std::begin(i2cResponse), std::end(i2cResponse), outData.begin());
            return I2CResult::OK;
        }));

        const auto status = comm.GetTransmitterTelemetry(telemetry);
        ASSERT_THAT(telemetry.Uptime, Eq(27h + 20min + 5s));
        ASSERT_THAT(telemetry.TransmitterBitRate, Eq(static_cast<Bitrate>(0)));

        ASSERT_THAT(telemetry.LastTransmittedRFReflectedPower, Eq(1057));
        ASSERT_THAT(telemetry.LastTransmittedAmplifierTemperature, Eq(1200));
        ASSERT_THAT(telemetry.LastTransmittedRFForwardPower, Eq(1345));
        ASSERT_THAT(telemetry.LastTransmittedTransmitterCurrentConsumption, Eq(1410));

        ASSERT_THAT(telemetry.NowRFForwardPower, Eq(3145));
        ASSERT_THAT(telemetry.NowTransmitterCurrentConsumption, Eq(4010));

        ASSERT_THAT(telemetry.StateWhenIdle, Eq(IdleState::Off));
        ASSERT_THAT(telemetry.BeaconState, Eq(false));

        ASSERT_THAT(status, Eq(false));
        ASSERT_THAT(error_counter, Eq(5));
    }

    TEST_F(CommTest, TestTransmitterTelemetryFailureGetLastTransmission)
    {
        TransmitterTelemetry telemetry;

        uint8_t uptimeRaw[] = {5, 20, 3, 1};
        uint8_t stateRaw[] = {0b00001011};
        uint8_t nowRaw[] = {0x00, 0x00, 0x11, 0x11, 0x49, 0x0C, 0xAA, 0x0F};

        gsl::span<std::uint8_t> i2cResponse;

        EXPECT_CALL(i2c, Write(TransmitterAddress, ElementsAre(TransmitterGetUptime)))
            .WillOnce(DoAll(Assign(&i2cResponse, gsl::make_span(uptimeRaw)), Return(I2CResult::OK)));

        EXPECT_CALL(i2c, Write(TransmitterAddress, ElementsAre(TransmitterGetState)))
            .WillOnce(DoAll(Assign(&i2cResponse, gsl::make_span(stateRaw)), Return(I2CResult::OK)));

        EXPECT_CALL(i2c, Write(TransmitterAddress, ElementsAre(TransmitterGetTelemetryLastTransmission))).WillOnce(Return(I2CResult::Nack));

        EXPECT_CALL(i2c, Write(TransmitterAddress, ElementsAre(TransmitterGetTelemetryInstant)))
            .WillOnce(DoAll(Assign(&i2cResponse, gsl::make_span(nowRaw)), Return(I2CResult::OK)));

        ON_CALL(i2c, Read(TransmitterAddress, _)).WillByDefault(Invoke([&](uint8_t /*address*/, span<uint8_t> outData) {
            std::fill(outData.begin(), outData.end(), 0);
            std::copy(std::begin(i2cResponse), std::end(i2cResponse), outData.begin());
            return I2CResult::OK;
        }));

        const auto status = comm.GetTransmitterTelemetry(telemetry);
        ASSERT_THAT(telemetry.Uptime, Eq(27h + 20min + 5s));
        ASSERT_THAT(telemetry.TransmitterBitRate, Eq(Bitrate::Comm4800bps));

        ASSERT_THAT(telemetry.LastTransmittedRFReflectedPower, Eq(0));
        ASSERT_THAT(telemetry.LastTransmittedAmplifierTemperature, Eq(0));
        ASSERT_THAT(telemetry.LastTransmittedRFForwardPower, Eq(0));
        ASSERT_THAT(telemetry.LastTransmittedTransmitterCurrentConsumption, Eq(0));

        ASSERT_THAT(telemetry.NowRFForwardPower, Eq(3145));
        ASSERT_THAT(telemetry.NowTransmitterCurrentConsumption, Eq(4010));

        ASSERT_THAT(telemetry.StateWhenIdle, Eq(IdleState::On));
        ASSERT_THAT(telemetry.BeaconState, Eq(true));

        ASSERT_THAT(status, Eq(false));
        ASSERT_THAT(error_counter, Eq(5));
    }

    TEST_F(CommTest, TestTransmitterTelemetryFailureGetInstant)
    {
        TransmitterTelemetry telemetry;

        uint8_t uptimeRaw[] = {5, 20, 3, 1};
        uint8_t stateRaw[] = {0b00001011};
        uint8_t lastTransmissionRaw[] = {0x21, 0x04, 0xB0, 0x04, 0x41, 0x05, 0x82, 0x05};

        gsl::span<std::uint8_t> i2cResponse;

        EXPECT_CALL(i2c, Write(TransmitterAddress, ElementsAre(TransmitterGetUptime)))
            .WillOnce(DoAll(Assign(&i2cResponse, gsl::make_span(uptimeRaw)), Return(I2CResult::OK)));

        EXPECT_CALL(i2c, Write(TransmitterAddress, ElementsAre(TransmitterGetState)))
            .WillOnce(DoAll(Assign(&i2cResponse, gsl::make_span(stateRaw)), Return(I2CResult::OK)));

        EXPECT_CALL(i2c, Write(TransmitterAddress, ElementsAre(TransmitterGetTelemetryLastTransmission)))
            .WillOnce(DoAll(Assign(&i2cResponse, gsl::make_span(lastTransmissionRaw)), Return(I2CResult::OK)));

        EXPECT_CALL(i2c, Write(TransmitterAddress, ElementsAre(TransmitterGetTelemetryInstant))).WillOnce(Return(I2CResult::Nack));

        ON_CALL(i2c, Read(TransmitterAddress, _)).WillByDefault(Invoke([&](uint8_t /*address*/, span<uint8_t> outData) {
            std::fill(outData.begin(), outData.end(), 0);
            std::copy(std::begin(i2cResponse), std::end(i2cResponse), outData.begin());
            return I2CResult::OK;
        }));

        const auto status = comm.GetTransmitterTelemetry(telemetry);
        ASSERT_THAT(telemetry.Uptime, Eq(27h + 20min + 5s));
        ASSERT_THAT(telemetry.TransmitterBitRate, Eq(Bitrate::Comm4800bps));

        ASSERT_THAT(telemetry.LastTransmittedRFReflectedPower, Eq(1057));
        ASSERT_THAT(telemetry.LastTransmittedAmplifierTemperature, Eq(1200));
        ASSERT_THAT(telemetry.LastTransmittedRFForwardPower, Eq(1345));
        ASSERT_THAT(telemetry.LastTransmittedTransmitterCurrentConsumption, Eq(1410));

        ASSERT_THAT(telemetry.NowRFForwardPower, Eq(0));
        ASSERT_THAT(telemetry.NowTransmitterCurrentConsumption, Eq(0));

        ASSERT_THAT(telemetry.StateWhenIdle, Eq(IdleState::On));
        ASSERT_THAT(telemetry.BeaconState, Eq(true));

        ASSERT_THAT(status, Eq(false));
        ASSERT_THAT(error_counter, Eq(5));
    }

    TEST_F(CommTest, TestGetTelemetry)
    {
        CommTelemetry telemetry;
        EXPECT_CALL(i2c, Write(TransmitterAddress, ElementsAre(TransmitterGetTelemetryLastTransmission))).WillOnce(Return(I2CResult::OK));
        EXPECT_CALL(i2c, Write(TransmitterAddress, ElementsAre(TransmitterGetTelemetryInstant))).WillOnce(Return(I2CResult::OK));
        EXPECT_CALL(i2c, Write(TransmitterAddress, ElementsAre(TransmitterGetUptime))).WillOnce(Return(I2CResult::OK));
        EXPECT_CALL(i2c, Write(TransmitterAddress, ElementsAre(TransmitterGetState))).WillOnce(Return(I2CResult::OK));
        EXPECT_CALL(i2c, Write(ReceiverAddress, ElementsAre(ReceiverGetTelemetry))).WillOnce(Return(I2CResult::OK));
        EXPECT_CALL(i2c, Write(ReceiverAddress, ElementsAre(ReceiverGetUptime))).WillOnce(Return(I2CResult::OK));
        const auto status = comm.GetTelemetry(telemetry);
        ASSERT_THAT(status, Eq(true));
        ASSERT_THAT(error_counter, Eq(0));
    }

    TEST_F(CommTest, TestSetBeaconTransmittFailure)
    {
        std::uint8_t buffer[1];
        Beacon beacon(1s, buffer);
        EXPECT_CALL(i2c, Write(TransmitterAddress, _)).WillOnce(Return(I2CResult::Failure));
        const auto status = comm.SetBeacon(beacon);
        ASSERT_THAT(status, Eq(false));
        ASSERT_THAT(error_counter, Eq(5));
    }

    TEST_F(CommTest, TestSetBeaconTransmitterRejected)
    {
        ExpectSendFrame(0xff);
        std::uint8_t buffer[1];
        Beacon beacon(1s, buffer);
        const auto status = comm.SetBeacon(beacon);
        ASSERT_THAT(status, Eq(false));
        ASSERT_THAT(error_counter, Eq(5));
    }

    TEST_F(CommTest, TestSetBeaconTransmitterBufferNotEmpty)
    {
        EXPECT_CALL(i2c, Write(TransmitterAddress, BeginsWith(TransmitterSetBeacon))).Times(0);
        ExpectSendFrame(38);

        std::uint8_t buffer[1];
        Beacon beacon(1s, buffer);
        const auto status = comm.SetBeacon(beacon);
        ASSERT_THAT(status.HasValue, Eq(true));
        ASSERT_THAT(error_counter, Eq(0));
    }

    TEST_F(CommTest, TestSetBeaconTransmitterBufferFull)
    {
        EXPECT_CALL(i2c, Write(TransmitterAddress, BeginsWith(TransmitterSetBeacon))).Times(0);
        ExpectSendFrame(0);

        std::uint8_t buffer[1];
        Beacon beacon(1s, buffer);
        const auto status = comm.SetBeacon(beacon);
        ASSERT_THAT(status.HasValue, Eq(true));
        ASSERT_THAT(error_counter, Eq(0));
    }

    TEST_F(CommTest, TestSetBeaconSizeOutOfRange)
    {
        EXPECT_CALL(i2c, Write(TransmitterAddress, BeginsWith(TransmitterSetBeacon))).Times(0);
        EXPECT_CALL(i2c, Read(TransmitterAddress, _)).Times(0);

        std::uint8_t buffer[MaxDownlinkFrameSize + 1];
        Beacon beacon(1s, buffer);
        const auto status = comm.SetBeacon(beacon);
        ASSERT_THAT(status, Eq(false));
        ASSERT_THAT(error_counter, Eq(0));
    }

    TEST_F(CommTest, TestSetBeacon)
    {
        EXPECT_CALL(i2c, Write(TransmitterAddress, BeginsWith(TransmitterSetBeacon))).Times(0);

        const uint8_t data[] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8};
        const uint8_t expectedFrame[] = {TransmitterSendFrame, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8};
        Beacon beacon(0x0a0bs, data);
        ExpectSendFrame(expectedFrame, 39);
        const auto status = comm.SetBeacon(beacon);
        ASSERT_THAT(status, Eq(true));
        ASSERT_THAT(error_counter, Eq(0));
    }

    TEST_F(CommTest, TestSetBeaconBufferEmpty)
    {
        EXPECT_CALL(i2c, Write(TransmitterAddress, BeginsWith(TransmitterSetBeacon))).Times(0);

        const uint8_t data[] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8};
        const uint8_t expectedFrame[] = {TransmitterSendFrame, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8};
        Beacon beacon(0x0a0bs, data);
        ExpectSendFrame(expectedFrame, 40);
        const auto status = comm.SetBeacon(beacon);
        ASSERT_THAT(status, Eq(true));
        ASSERT_THAT(error_counter, Eq(0));
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
        ASSERT_THAT(error_counter, Eq(5));
    }

    TEST_F(CommTest, TestTransmitterWatchdogReset)
    {
        i2c.ExpectWriteCommand(TransmitterAddress, TransmitterWatchdogReset).WillOnce(Return(I2CResult::OK));
        ASSERT_THAT(comm.ResetWatchdogTransmitter(), Eq(true));
        ASSERT_THAT(error_counter, Eq(0));
    }

    TEST_F(CommTest, TestReceiverWatchdogResetFailure)
    {
        i2c.ExpectWriteCommand(ReceiverAddress, ReceiverWatchdogReset).WillOnce(Return(I2CResult::Nack));
        ASSERT_THAT(comm.ResetWatchdogReceiver(), Eq(false));
        ASSERT_THAT(error_counter, Eq(5));
    }

    TEST_F(CommTest, TestReceiverWatchdogReset)
    {
        i2c.ExpectWriteCommand(ReceiverAddress, ReceiverWatchdogReset).WillOnce(Return(I2CResult::OK));
        ASSERT_THAT(comm.ResetWatchdogReceiver(), Eq(true));
        ASSERT_THAT(error_counter, Eq(0));
    }

    TEST_F(CommTest, TestPollHardwareQueriesFrameCountNoFrames)
    {
        i2c.ExpectWriteCommand(ReceiverAddress, ReceiverWatchdogReset).WillOnce(Return(I2CResult::OK));
        MockFrameCount(0);
        auto result = comm.PollHardware();
        ASSERT_THAT(result, Eq(false));
        ASSERT_THAT(error_counter, Eq(0));
    }

    TEST_F(CommTest, TestPollHardwareProcessesValidFrames)
    {
        std::uint8_t buffer[10] = {0};
        i2c.ExpectWriteCommand(ReceiverAddress, ReceiverWatchdogReset).WillOnce(Return(I2CResult::OK));
        MockFrameCount(1);
        MockFrame(buffer, 1, 2);
        MockRemoveFrame(I2CResult::OK);
        EXPECT_CALL(frameHandler, HandleFrame(_, _)).Times(1);
        auto result = comm.PollHardware();
        ASSERT_THAT(result, Eq(true));
        ASSERT_THAT(error_counter, Eq(0));
    }

    TEST_F(CommTest, TestPollHardwareInValidFramesAreRemoved)
    {
        std::uint8_t buffer[10] = {0};
        MockFrameCount(1);
        MockFrame(buffer, 0xffff, 0xffff);
        MockRemoveFrame(I2CResult::OK);
        i2c.ExpectWriteCommand(ReceiverAddress, ReceiverWatchdogReset).WillOnce(Return(I2CResult::OK));
        comm.PollHardware();

        ASSERT_THAT(error_counter, Eq(0));
    }

    TEST_F(CommTest, TestPollHardwareRemoveFrameOnReceiveError)
    {
        MockFrameCount(1);
        EXPECT_CALL(i2c, Write(ReceiverAddress, ElementsAre(ReceiverGetFrame))).WillRepeatedly(Return(I2CResult::Nack));
        MockRemoveFrame(I2CResult::OK);
        i2c.ExpectWriteCommand(ReceiverAddress, ReceiverWatchdogReset).WillOnce(Return(I2CResult::OK));
        comm.PollHardware();

        // Frame receive will fail, but reset will succeed
        ASSERT_THAT(error_counter, Eq(3));
    }

    TEST_F(CommTest, TestPollHardwareDoNotTryToReceiveFrameOnQueryFailure)
    {
        i2c.ExpectWriteCommand(ReceiverAddress, ReceiverWatchdogReset).WillOnce(Return(I2CResult::OK));
        EXPECT_CALL(i2c, Write(ReceiverAddress, ElementsAre(ReceiverGetFrameCount))).WillOnce(Return(I2CResult::Nack));

        comm.PollHardware();

        ASSERT_THAT(error_counter, Eq(3));
    }

    TEST_F(CommTest, TestPollHardwareResetsWatchdogOnReceiver)
    {
        EXPECT_CALL(i2c, Write(ReceiverAddress, ElementsAre(ReceiverGetFrameCount))).WillOnce(Return(I2CResult::Nack));
        i2c.ExpectWriteCommand(ReceiverAddress, ReceiverWatchdogReset).WillOnce(Return(I2CResult::OK));
        comm.PollHardware();

        ASSERT_THAT(error_counter, Eq(3));
    }

    TEST_F(CommTest, TestPollHardwareResetsWatchdogOnTransmitter)
    {
        EXPECT_CALL(i2c, Write(ReceiverAddress, ElementsAre(ReceiverGetFrameCount))).WillOnce(Return(I2CResult::Nack));
        i2c.ExpectWriteCommand(ReceiverAddress, ReceiverWatchdogReset).WillOnce(Return(I2CResult::Nack));
        i2c.ExpectWriteCommand(TransmitterAddress, TransmitterWatchdogReset).WillOnce(Return(I2CResult::OK));
        comm.PollHardware();

        ASSERT_THAT(error_counter, Eq(8));
    }

    TEST_F(CommTest, TestRestartHardwareFailure)
    {
        i2c.ExpectWriteCommand(ReceiverAddress, HardwareReset).WillOnce(Return(I2CResult::Nack));
        ASSERT_THAT(comm.RestartHardware(), Eq(false));
        ASSERT_THAT(error_counter, Eq(5));
    }

    TEST_F(CommTest, TestRestartHardware)
    {
        i2c.ExpectWriteCommand(ReceiverAddress, HardwareReset).WillOnce(Return(I2CResult::OK));
        ASSERT_THAT(comm.RestartHardware(), Eq(true));
        ASSERT_THAT(error_counter, Eq(0));
    }

    TEST_F(CommTest, TestStartTask)
    {
        EXPECT_CALL(system, ResumeTask(_));
        ASSERT_THAT(comm.StartTask(), Eq(true));
    }

    TEST_F(CommTest, TestRestartFailIfAlreadyRunning)
    {
        ON_CALL(system, EventGroupGetBits(_)).WillByDefault(Return(4));
        EXPECT_CALL(system, ResumeTask(_)).Times(0);
        ASSERT_THAT(comm.StartTask(), Eq(false));
    }

    TEST_F(CommTest, DoNotTriggerTransmitterResetIfQueueIsServedOnTime)
    {
        std::chrono::milliseconds uptime = 30s;

        ON_CALL(system, GetUptime()).WillByDefault(ReturnPointee(&uptime));
        {
            InSequence s;
            ExpectSendFrame(12, [&uptime]() { uptime = 30s; });
            ExpectSendFrame(12, [&uptime]() { uptime += 10s; });
            i2c.ExpectWriteCommand(TransmitterAddress, TransmitterReset).Times(0);
        }

        const std::uint8_t frame[] = {1, 2, 3};

        comm.SendFrame(frame);
        comm.SendFrame(frame);
    }

    TEST_F(CommTest, TriggerTransmitterResetIfQueueIsStalled)
    {
        std::chrono::milliseconds uptime = 30s;

        ON_CALL(system, GetUptime()).WillByDefault(ReturnPointee(&uptime));
        {
            InSequence s;
            ExpectSendFrame(12, [&uptime]() { uptime = 30s; });
            ExpectSendFrame(11, [&uptime]() { uptime += 15s; });
            i2c.ExpectWriteCommand(TransmitterAddress, TransmitterReset);
            ExpectSendFrame(11, [&uptime]() { uptime += 10s; });
            i2c.ExpectWriteCommand(TransmitterAddress, TransmitterReset).Times(0);
        }

        const std::uint8_t frame[] = {1, 2, 3};

        comm.SendFrame(frame);
        comm.SendFrame(frame);
        comm.SendFrame(frame);
    }
}
