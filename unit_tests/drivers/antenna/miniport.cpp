#include "antenna/miniport.h"
#include <cstdint>
#include <gsl/span>
#include <tuple>
#include "gtest/gtest.h"
#include "I2C/I2CMock.hpp"
#include "antenna/antenna.h"
#include "time/TimeSpan.hpp"
#include "utils.hpp"

static constexpr std::uint8_t DeployAntenna1 = 0xa1;
static constexpr std::uint8_t DeployAntenna2 = 0xa2;
static constexpr std::uint8_t DeployAntenna3 = 0xa3;
static constexpr std::uint8_t DeployAntenna4 = 0xa4;
static constexpr std::uint8_t StartDeployment = 0xa5;
static constexpr std::uint8_t CancelDeployment = 0xa9;
static constexpr std::uint8_t Reset = 0xaa;
static constexpr std::uint8_t Disarm = 0xac;
static constexpr std::uint8_t Arm = 0xad;
static constexpr std::uint8_t DeployAntenna1Override = 0xba;
static constexpr std::uint8_t DeployAntenna2Override = 0xbb;
static constexpr std::uint8_t DeployAntenna3Override = 0xbc;
static constexpr std::uint8_t DeployAntenna4Override = 0xbd;
static constexpr std::uint8_t QueryActivationCount1 = 0xb0;
static constexpr std::uint8_t QueryActivationCount2 = 0xb1;
static constexpr std::uint8_t QueryActivationCount3 = 0xb2;
static constexpr std::uint8_t QueryActivationCount4 = 0xb3;
static constexpr std::uint8_t QueryActivationTime1 = 0xb4;
static constexpr std::uint8_t QueryActivationTime2 = 0xb5;
static constexpr std::uint8_t QueryActivationTime3 = 0xb6;
static constexpr std::uint8_t QueryActivationTime4 = 0xb7;
static constexpr std::uint8_t QueryTemperature = 0xc0;
static constexpr std::uint8_t QueryDeploymentStatus = 0xc3;

using testing::Return;
using testing::_;
using testing::Eq;
using testing::Ge;
using testing::Ne;
using testing::Invoke;
using testing::ElementsAre;
using gsl::span;
using drivers::i2c::I2CResult;
using namespace std::chrono_literals;
namespace
{
    class AntennaMiniportTest : public testing::Test
    {
      protected:
        AntennaMiniportTest();
        I2CBusMock i2c;
        AntennaMiniportDriver miniport;
    };

    AntennaMiniportTest::AntennaMiniportTest()
    {
    }

    TEST_F(AntennaMiniportTest, TestHardwareReset)
    {
        i2c.ExpectWriteCommand(ANTENNA_PRIMARY_CHANNEL, Reset).WillOnce(Return(I2CResult::OK));
        const auto status = miniport.Reset(&i2c, ANTENNA_PRIMARY_CHANNEL);
        ASSERT_THAT(status, Eq(OSResult::Success));
    }

    TEST_F(AntennaMiniportTest, TestHardwareResetFailure)
    {
        i2c.ExpectWriteCommand(ANTENNA_PRIMARY_CHANNEL, Reset).WillOnce(Return(I2CResult::Nack));
        const auto status = miniport.Reset(&i2c, ANTENNA_PRIMARY_CHANNEL);
        ASSERT_THAT(status, Ne(OSResult::Success));
    }

    TEST_F(AntennaMiniportTest, TestArmingDeployment)
    {
        i2c.ExpectWriteCommand(ANTENNA_PRIMARY_CHANNEL, Arm).WillOnce(Return(I2CResult::OK));
        const auto status = miniport.ArmDeploymentSystem(&i2c, ANTENNA_PRIMARY_CHANNEL);
        ASSERT_THAT(status, Eq(OSResult::Success));
    }

    TEST_F(AntennaMiniportTest, TestArmingDeploymentFailure)
    {
        i2c.ExpectWriteCommand(ANTENNA_PRIMARY_CHANNEL, Arm).WillOnce(Return(I2CResult::Nack));
        const auto status = miniport.ArmDeploymentSystem(&i2c, ANTENNA_PRIMARY_CHANNEL);
        ASSERT_THAT(status, Ne(OSResult::Success));
    }

    TEST_F(AntennaMiniportTest, TestDisarmingDeployment)
    {
        i2c.ExpectWriteCommand(ANTENNA_PRIMARY_CHANNEL, Disarm).WillOnce(Return(I2CResult::OK));
        const auto status = miniport.DisarmDeploymentSystem(&i2c, ANTENNA_PRIMARY_CHANNEL);
        ASSERT_THAT(status, Eq(OSResult::Success));
    }

    TEST_F(AntennaMiniportTest, TestDisarmingDeploymentFailure)
    {
        i2c.ExpectWriteCommand(ANTENNA_PRIMARY_CHANNEL, Disarm).WillOnce(Return(I2CResult::Nack));
        const auto status = miniport.DisarmDeploymentSystem(&i2c, ANTENNA_PRIMARY_CHANNEL);
        ASSERT_THAT(status, Ne(OSResult::Success));
    }

    TEST_F(AntennaMiniportTest, TestAutomaticDeployment)
    {
        EXPECT_CALL(i2c, Write(ANTENNA_PRIMARY_CHANNEL, testing::ElementsAre(StartDeployment, 50))).WillOnce(Return(I2CResult::OK));
        const auto status = miniport.InitializeAutomaticDeployment(&i2c, ANTENNA_PRIMARY_CHANNEL, 200s);
        ASSERT_THAT(status, Eq(OSResult::Success));
    }

    TEST_F(AntennaMiniportTest, TestAutomaticDeploymentFailure)
    {
        EXPECT_CALL(i2c, Write(ANTENNA_PRIMARY_CHANNEL, BeginsWith(StartDeployment))).WillOnce(Return(I2CResult::Nack));
        const auto status = miniport.InitializeAutomaticDeployment(&i2c, ANTENNA_PRIMARY_CHANNEL, 200s);
        ASSERT_THAT(status, Ne(OSResult::Success));
    }

    TEST_F(AntennaMiniportTest, TestCancelAutomaticDeployment)
    {
        i2c.ExpectWriteCommand(ANTENNA_PRIMARY_CHANNEL, CancelDeployment).WillOnce(Return(I2CResult::OK));
        const auto status = miniport.CancelAntennaDeployment(&i2c, ANTENNA_PRIMARY_CHANNEL);
        ASSERT_THAT(status, Eq(OSResult::Success));
    }

    TEST_F(AntennaMiniportTest, TestCancelAutomaticDeploymentFailure)
    {
        i2c.ExpectWriteCommand(ANTENNA_PRIMARY_CHANNEL, CancelDeployment).WillOnce(Return(I2CResult::Nack));
        const auto status = miniport.CancelAntennaDeployment(&i2c, ANTENNA_PRIMARY_CHANNEL);
        ASSERT_THAT(status, Ne(OSResult::Success));
    }

    TEST_F(AntennaMiniportTest, TestManualAntennaDeployment)
    {
        EXPECT_CALL(i2c, Write(ANTENNA_PRIMARY_CHANNEL, ElementsAre(DeployAntenna1, 200u))).WillOnce(Return(I2CResult::OK));
        const auto status = miniport.DeployAntenna(&i2c, ANTENNA_PRIMARY_CHANNEL, ANTENNA1_ID, 200s, false);
        ASSERT_THAT(status, Eq(OSResult::Success));
    }

    TEST_F(AntennaMiniportTest, TestManualAntennaDeploymentFailure)
    {
        EXPECT_CALL(i2c, Write(ANTENNA_PRIMARY_CHANNEL, ElementsAre(DeployAntenna2, 200u))).WillOnce(Return(I2CResult::Nack));
        const auto status = miniport.DeployAntenna(&i2c, ANTENNA_PRIMARY_CHANNEL, ANTENNA2_ID, 200s, false);
        ASSERT_THAT(status, Ne(OSResult::Success));
    }

    TEST_F(AntennaMiniportTest, TestManualAntennaDeploymentWithOverride)
    {
        EXPECT_CALL(i2c, Write(ANTENNA_PRIMARY_CHANNEL, ElementsAre(DeployAntenna1Override, 200u))).WillOnce(Return(I2CResult::OK));
        const auto status = miniport.DeployAntenna(&i2c, ANTENNA_PRIMARY_CHANNEL, ANTENNA1_ID, 200s, true);
        ASSERT_THAT(status, Eq(OSResult::Success));
    }

    TEST_F(AntennaMiniportTest, TestManualAntennaDeploymentWithOverrideFailure)
    {
        EXPECT_CALL(i2c, Write(ANTENNA_BACKUP_CHANNEL, ElementsAre(DeployAntenna3Override, 200u))).WillOnce(Return(I2CResult::Nack));
        const auto status = miniport.DeployAntenna(&i2c, ANTENNA_BACKUP_CHANNEL, ANTENNA3_ID, 200s, true);
        ASSERT_THAT(status, Ne(OSResult::Success));
    }

    TEST_F(AntennaMiniportTest, TestAntennaActivationCount)
    {
        EXPECT_CALL(i2c, WriteRead(ANTENNA_PRIMARY_CHANNEL, ElementsAre(QueryActivationCount1), _))
            .WillOnce(Invoke([=](uint8_t /*address*/, span<const uint8_t> /*inData*/, span<uint8_t> outData) {
                std::fill(outData.begin(), outData.end(), 0);
                outData[0] = 10;
                return I2CResult::OK;
            }));
        uint8_t response;
        const auto status = miniport.GetAntennaActivationCount(&i2c, ANTENNA_PRIMARY_CHANNEL, ANTENNA1_ID, &response);
        ASSERT_THAT(status, Eq(OSResult::Success));
        ASSERT_THAT(response, Eq(10u));
    }

    TEST_F(AntennaMiniportTest, TestAntennaActivationCountFailure)
    {
        EXPECT_CALL(i2c, WriteRead(ANTENNA_BACKUP_CHANNEL, ElementsAre(QueryActivationCount2), _)).WillOnce(Return(I2CResult::Nack));
        uint8_t response;
        const auto status = miniport.GetAntennaActivationCount(&i2c, ANTENNA_BACKUP_CHANNEL, ANTENNA2_ID, &response);
        ASSERT_THAT(status, Ne(OSResult::Success));
        ASSERT_THAT(response, Eq(0u));
    }

    TEST_F(AntennaMiniportTest, TestAntennaTemperature)
    {
        EXPECT_CALL(i2c, WriteRead(ANTENNA_PRIMARY_CHANNEL, ElementsAre(QueryTemperature), _))
            .WillOnce(Invoke([=](uint8_t /*address*/, span<const uint8_t> /*inData*/, span<uint8_t> outData) {
                std::fill(outData.begin(), outData.end(), 0);
                outData[0] = 0;
                outData[1] = 0x11;
                return I2CResult::OK;
            }));
        uint16_t response = 0;
        const auto status = miniport.GetTemperature(&i2c, ANTENNA_PRIMARY_CHANNEL, &response);
        ASSERT_THAT(status, Eq(OSResult::Success));
        ASSERT_THAT(response, Eq(0x11));
    }

    TEST_F(AntennaMiniportTest, TestAntennaTemperatureOutOfRange)
    {
        EXPECT_CALL(i2c, WriteRead(ANTENNA_PRIMARY_CHANNEL, ElementsAre(QueryTemperature), _))
            .WillOnce(Invoke([=](uint8_t /*address*/, span<const uint8_t> /*inData*/, span<uint8_t> outData) {
                std::fill(outData.begin(), outData.end(), 0);
                outData[0] = 0xfc;
                outData[1] = 0;
                return I2CResult::OK;
            }));
        uint16_t response;
        const auto status = miniport.GetTemperature(&i2c, ANTENNA_PRIMARY_CHANNEL, &response);
        ASSERT_THAT(status, Eq(OSResult::OutOfRange));
        ASSERT_THAT(response, Eq(0));
    }

    TEST_F(AntennaMiniportTest, TestAntennaTemperatureFailure)
    {
        EXPECT_CALL(i2c, WriteRead(ANTENNA_PRIMARY_CHANNEL, ElementsAre(QueryTemperature), _)).WillOnce(Return(I2CResult::Nack));
        uint16_t response;
        const auto status = miniport.GetTemperature(&i2c, ANTENNA_PRIMARY_CHANNEL, &response);
        ASSERT_THAT(status, Ne(OSResult::Success));
        ASSERT_THAT(response, Eq(0u));
    }

    TEST_F(AntennaMiniportTest, TestAntennaActivationTime)
    {
        EXPECT_CALL(i2c, WriteRead(ANTENNA_PRIMARY_CHANNEL, ElementsAre(QueryActivationTime1), _))
            .WillOnce(Invoke([=](uint8_t /*address*/, span<const uint8_t> /*inData*/, span<uint8_t> outData) {
                std::fill(outData.begin(), outData.end(), 0);
                outData[0] = 10;
                return I2CResult::OK;
            }));
        std::chrono::milliseconds response;
        const auto status = miniport.GetAntennaActivationTime(&i2c, ANTENNA_PRIMARY_CHANNEL, ANTENNA1_ID, &response);
        ASSERT_THAT(status, Eq(OSResult::Success));
        ASSERT_THAT(response, Eq(128000ms));
    }

    TEST_F(AntennaMiniportTest, TestAntennaActivationTimeFailure)
    {
        EXPECT_CALL(i2c, WriteRead(ANTENNA_PRIMARY_CHANNEL, ElementsAre(QueryActivationTime2), _)).WillOnce(Return(I2CResult::Nack));
        std::chrono::milliseconds response;
        const auto status = miniport.GetAntennaActivationTime(&i2c, ANTENNA_PRIMARY_CHANNEL, ANTENNA2_ID, &response);
        ASSERT_THAT(status, Ne(OSResult::Success));
        ASSERT_THAT(response, Eq(0ms));
    }

    class AntennaDeploymentStatusTest
        : public testing::TestWithParam<std::tuple<uint8_t, uint8_t, I2CResult, OSResult, uint32_t, uint32_t, bool, bool>>
    {
      public:
        AntennaDeploymentStatusTest();
        void MockI2C();

      protected:
        I2CBusMock i2c;
        AntennaMiniportDriver miniport;
    };

    AntennaDeploymentStatusTest::AntennaDeploymentStatusTest()
    {
    }

    void AntennaDeploymentStatusTest::MockI2C()
    {
        const auto response1 = std::get<0>(GetParam());
        const auto response2 = std::get<1>(GetParam());
        const auto i2cResult = std::get<2>(GetParam());

        EXPECT_CALL(i2c, WriteRead(ANTENNA_PRIMARY_CHANNEL, ElementsAre(QueryDeploymentStatus), _))
            .WillOnce(Invoke([=](uint8_t /*address*/, span<const uint8_t> /*inData*/, span<uint8_t> outData) {
                std::fill(outData.begin(), outData.end(), 0);
                outData[0] = response1;
                outData[1] = response2;
                return i2cResult;
            }));
    }

    TEST_P(AntennaDeploymentStatusTest, TestGetDeploymentStatus)
    {
        MockI2C();

        AntennaDeploymentStatus response;
        const auto expectedResult = std::get<3>(GetParam());

        const auto result = miniport.GetDeploymentStatus(&i2c, ANTENNA_PRIMARY_CHANNEL, &response);
        ASSERT_THAT(result, Eq(expectedResult));
    }

    TEST_P(AntennaDeploymentStatusTest, TestDeploymentStatusData)
    {
        MockI2C();
        AntennaDeploymentStatus response;
        const auto deploymentStatuses = std::get<4>(GetParam());
        const auto deplomentActive = std::get<5>(GetParam());
        const auto overrideActive = std::get<6>(GetParam());
        const auto systemArmed = std::get<7>(GetParam());
        miniport.GetDeploymentStatus(&i2c, ANTENNA_PRIMARY_CHANNEL, &response);

        ASSERT_THAT(response.DeploymentStatus[0], Eq((deploymentStatuses & 0xff) != 0));
        ASSERT_THAT(response.DeploymentStatus[1], Eq((deploymentStatuses & 0xff00) != 0));
        ASSERT_THAT(response.DeploymentStatus[2], Eq((deploymentStatuses & 0xff0000) != 0));
        ASSERT_THAT(response.DeploymentStatus[3], Eq((deploymentStatuses & 0xff000000) != 0));

        ASSERT_THAT(response.IsDeploymentActive[0], Eq((deplomentActive & 0xff) != 0));
        ASSERT_THAT(response.IsDeploymentActive[1], Eq((deplomentActive & 0xff00) != 0));
        ASSERT_THAT(response.IsDeploymentActive[2], Eq((deplomentActive & 0xff0000) != 0));
        ASSERT_THAT(response.IsDeploymentActive[3], Eq((deplomentActive & 0xff000000) != 0));

        ASSERT_THAT(response.IgnoringDeploymentSwitches, Eq(overrideActive));
        ASSERT_THAT(response.DeploymentSystemArmed, Eq(systemArmed));
    }

    INSTANTIATE_TEST_CASE_P(AntennaDeploymentStatusTestSet,
        AntennaDeploymentStatusTest,
        testing::Values(std::make_tuple(0, 0, I2CResult::Nack, OSResult::IOError, 0u, 0u, false, false),
            std::make_tuple(0, 0, I2CResult::OK, OSResult::Success, 0x01010101u, 0u, false, false),
            std::make_tuple(1, 0, I2CResult::OK, OSResult::Success, 0x01010101u, 0u, false, true),
            std::make_tuple(0, 1, I2CResult::OK, OSResult::Success, 0x01010101u, 0u, true, false),
            std::make_tuple(2, 0, I2CResult::OK, OSResult::Success, 0x01010101u, 0x1000000, false, false),
            std::make_tuple(8, 0, I2CResult::OK, OSResult::Success, 0x00010101u, 0u, false, false),
            std::make_tuple(0x0a, 0, I2CResult::OK, OSResult::Success, 0x00010101u, 0x1000000, false, false),
            std::make_tuple(0x20, 0, I2CResult::OK, OSResult::Success, 0x01010101u, 0x10000, false, false),
            std::make_tuple(0x80, 0, I2CResult::OK, OSResult::Success, 0x01000101u, 0u, false, false),
            std::make_tuple(0xa0, 0, I2CResult::OK, OSResult::Success, 0x01000101u, 0x10000, false, false),
            std::make_tuple(0, 2, I2CResult::OK, OSResult::Success, 0x01010101u, 0x100, false, false),
            std::make_tuple(0, 8, I2CResult::OK, OSResult::Success, 0x01010001u, 0u, false, false),
            std::make_tuple(0, 0x0a, I2CResult::OK, OSResult::Success, 0x01010001u, 0x100, false, false),
            std::make_tuple(0, 0x20, I2CResult::OK, OSResult::Success, 0x01010101u, 0x1, false, false),
            std::make_tuple(0, 0x80, I2CResult::OK, OSResult::Success, 0x01010100u, 0u, false, false),
            std::make_tuple(0, 0xa0, I2CResult::OK, OSResult::Success, 0x01010100u, 0x1, false, false),
            std::make_tuple(0, 0x10, I2CResult::OK, OSResult::OutOfRange, 0x0, 0u, false, false)), );
}
