#include "payload.h"
#include <em_gpio.h>
#include "logger/logger.h"

using namespace drivers::payload;
using drivers::i2c::I2CResult;

PayloadDriver::PayloadDriver(drivers::i2c::II2CBus& communicationBus, const drivers::gpio::Pin& interruptPin)
    : _i2c(communicationBus),      //
      _interruptPin(interruptPin), //
      _sync(nullptr)               //
{
}

void PayloadDriver::IRQHandler()
{
    GPIO_IntClear(IRQMask());
    auto value = _interruptPin.Input();
    if (!value)
    {
        System::GiveSemaphoreISR(_sync);
    }

    System::EndSwitchingISR();
}

void PayloadDriver::Initialize()
{
    _sync = System::CreateBinarySemaphore();

    auto interruptBank = _interruptPin.PinNumber() % 2 ? GPIO_ODD_IRQn : GPIO_EVEN_IRQn;

    NVIC_SetPriority(interruptBank, InterruptPriority);
    NVIC_EnableIRQ(interruptBank);
}

bool PayloadDriver::IsBusy() const
{
    return _interruptPin.Input();
}

OSResult PayloadDriver::PayloadRead(gsl::span<std::uint8_t> outData, gsl::span<std::uint8_t> inData)
{
    auto result = _i2c.WriteRead(I2CAddress, outData, inData);
    const bool status = (result == I2CResult::OK);
    if (!status)
    {
        LOGF(LOG_LEVEL_ERROR, "Unable to perform Payload I2C Write and Read. Reason: %d", num(result));
        return OSResult::InvalidOperation;
    }

    return OSResult::Success;
}
OSResult PayloadDriver::PayloadWrite(gsl::span<std::uint8_t> outData)
{
    auto result = _i2c.Write(I2CAddress, outData);
    const bool status = (result == I2CResult::OK);
    if (!status)
    {
        LOGF(LOG_LEVEL_ERROR, "Unable to perform Payload I2C Write. Reason: %d", num(result));
        return OSResult::InvalidOperation;
    }

    return OSResult::Success;
}

OSResult PayloadDriver::WaitForData()
{
    auto result = System::TakeSemaphore(_sync, std::chrono::milliseconds(DefaultTimeout));
    if (result != OSResult::Success)
    {
        LOGF(LOG_LEVEL_ERROR, "Take semaphore for PAyload synchronisation failed. Reason: %d", num(result));
        return result;
    }

    return OSResult::Success;
}
