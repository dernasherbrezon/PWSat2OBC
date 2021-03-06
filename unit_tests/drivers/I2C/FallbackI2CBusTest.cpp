#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "gmock/gmock-matchers.h"
#include "I2C/I2CMock.hpp"
#include "i2c/wrappers.h"
#include "system.h"

using testing::Test;
using testing::_;
using testing::Return;
using testing::Eq;

using namespace drivers::i2c;
namespace
{
    class FallbackI2CBusTest : public Test
    {
      protected:
        testing::NiceMock<I2CBusMock> systemBus;
        testing::NiceMock<I2CBusMock> payloadBus;

        I2CInterface buses;

        I2CFallbackBus bus;

      public:
        FallbackI2CBusTest();
    };

    FallbackI2CBusTest::FallbackI2CBusTest() : buses(systemBus, payloadBus), bus(buses)
    {
    }

    TEST_F(FallbackI2CBusTest, WriteReadShouldNotFallbackToPayloadBusIfSystemBusWorked)
    {
        EXPECT_CALL(systemBus, WriteRead(_, _, _)).WillOnce(Return(I2CResult::OK));
        EXPECT_CALL(payloadBus, WriteRead(_, _, _)).Times(0);

        uint8_t in[] = {1, 2, 3};
        uint8_t out[3] = {0};

        auto r = bus.WriteRead(0x20, in, out);

        ASSERT_THAT(r, Eq(I2CResult::OK));
    }

    TEST_F(FallbackI2CBusTest, WriteReadShouldFallbackToPayloadBusIfSystemBusFailed)
    {
        EXPECT_CALL(systemBus, WriteRead(_, _, _)).WillOnce(Return(I2CResult::Failure));
        EXPECT_CALL(payloadBus, WriteRead(_, _, _)).WillOnce(Return(I2CResult::OK));

        uint8_t in[] = {1, 2, 3};
        uint8_t out[3] = {0};

        auto r = bus.WriteRead(0x20, in, out);

        ASSERT_THAT(r, Eq(I2CResult::OK));
    }

    TEST_F(FallbackI2CBusTest, WriteReadShouldFailWhenBothBusesFail)
    {
        EXPECT_CALL(systemBus, WriteRead(_, _, _)).WillOnce(Return(I2CResult::Failure));
        EXPECT_CALL(payloadBus, WriteRead(_, _, _)).WillOnce(Return(I2CResult::Nack));

        uint8_t in[] = {1, 2, 3};
        uint8_t out[3] = {0};

        auto r = bus.WriteRead(0x20, in, out);

        ASSERT_THAT(r, Eq(I2CResult::Nack));
    }

    TEST_F(FallbackI2CBusTest, WriteShouldNotFallbackToPayloadBusIfSystemBusWorked)
    {
        EXPECT_CALL(systemBus, Write(_, _)).WillOnce(Return(I2CResult::OK));
        EXPECT_CALL(payloadBus, Write(_, _)).Times(0);

        uint8_t in[] = {1, 2, 3};

        auto r = bus.Write(0x20, in);

        ASSERT_THAT(r, Eq(I2CResult::OK));
    }

    TEST_F(FallbackI2CBusTest, WriteShouldFallbackToPayloadBusIfSystemBusFailed)
    {
        EXPECT_CALL(systemBus, Write(_, _)).WillOnce(Return(I2CResult::Failure));
        EXPECT_CALL(payloadBus, Write(_, _)).WillOnce(Return(I2CResult::OK));

        uint8_t in[] = {1, 2, 3};

        auto r = bus.Write(0x20, in);

        ASSERT_THAT(r, Eq(I2CResult::OK));
    }

    TEST_F(FallbackI2CBusTest, WriteShouldFailWhenBothBusesFail)
    {
        EXPECT_CALL(systemBus, Write(_, _)).WillOnce(Return(I2CResult::Failure));
        EXPECT_CALL(payloadBus, Write(_, _)).WillOnce(Return(I2CResult::Nack));

        uint8_t in[] = {1, 2, 3};

        auto r = bus.Write(0x20, in);

        ASSERT_THAT(r, Eq(I2CResult::Nack));
    }
}
