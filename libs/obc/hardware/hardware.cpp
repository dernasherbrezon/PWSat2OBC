#include "hardware.h"

using drivers::i2c::II2CBus;
using drivers::i2c::I2CResult;
using drivers::i2c::I2CAddress;
using io_map::I2C;
using io_map::I2C_0;
using io_map::I2C_1;

using namespace obc;

I2CSingleBus::I2CSingleBus(I2C_TypeDef* hw,
    uint16_t location,
    GPIO_Port_TypeDef port,
    uint16_t sdaPin,
    uint16_t sclPin,
    CMU_Clock_TypeDef clock,
    IRQn_Type irq,
    PowerControl* powerControl)
    : //
      Driver(hw, location, port, sdaPin, sclPin, clock, irq),
      ErrorHandling(Driver, I2CErrorHandler, powerControl)
{
}

I2CResult I2CSingleBus::I2CErrorHandler(II2CBus& bus, I2CResult result, I2CAddress address, void* context)
{
    UNREFERENCED_PARAMETER(bus);
    UNREFERENCED_PARAMETER(address);

    auto power = reinterpret_cast<PowerControl*>(context);

    if (result == I2CResult::ClockLatched)
    {
        LOG(LOG_LEVEL_FATAL, "SCL latched. Triggering power cycle");
        power->TriggerSystemPowerCycle(power);
        return result;
    }

    return result;
}

OBCHardwareI2C::OBCHardwareI2C(PowerControl* powerControl)
    : //
      Peripherals{
          {I2C0, I2C_0::Location, I2C_0::SDA::Port, I2C_0::SDA::PinNumber, I2C_0::SCL::PinNumber, cmuClock_I2C0, I2C0_IRQn, powerControl},
          {I2C1, I2C_1::Location, I2C_1::SDA::Port, I2C_1::SDA::PinNumber, I2C_1::SCL::PinNumber, cmuClock_I2C1, I2C1_IRQn, powerControl} //
      },
      Buses(Peripherals[I2C::SystemBus].ErrorHandling, Peripherals[I2C::PayloadBus].ErrorHandling), //
      Fallback(Buses)                                                                               //
{
}

void OBCHardwareI2C::Initialize()
{
    this->Peripherals[0].Driver.Initialize();
    this->Peripherals[1].Driver.Initialize();
}

void OBCHardware::Initialize()
{
    this->Pins.Initialize();
    this->I2C.Initialize();
    this->SPI.Initialize();
}

OBCHardware::OBCHardware(PowerControl* powerControl) : I2C(powerControl)
{
}