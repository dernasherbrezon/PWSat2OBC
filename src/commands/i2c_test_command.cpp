#include <stdint.h>
#include <string.h>
#include <gsl/span>

#include "i2c/i2c.h"
#include "obc_access.hpp"
#include "system.h"
#include "terminal.h"
#include "terminal/terminal.h"

using gsl::span;
using drivers::i2c::II2CBus;
using drivers::i2c::I2CResult;

void I2CTestCommandHandler(uint16_t argc, char* argv[])
{
    UNREFERENCED_PARAMETER(argc);

    if (argc != 4)
    {
        GetTerminal().Puts("i2c <r|w|wr> <system|payload> <device> <data>\n");
        return;
    }

    II2CBus* bus;

    if (strcmp(argv[1], "system") == 0)
    {
        bus = &GetI2C().Bus;
    }
    else if (strcmp(argv[1], "payload") == 0)
    {
        bus = &GetI2C().Payload;
    }
    else
    {
        GetTerminal().Puts("Unknown bus\n");
        return;
    }

    const uint8_t device = (uint8_t)atoi(argv[2]);
    uint8_t* data = (uint8_t*)argv[3];
    const size_t dataLength = strlen(argv[3]);
    uint8_t output[100] = {0};

    I2CResult result;

    if (strcmp(argv[0], "wr") == 0)
    {
        result = bus->WriteRead(device, span<const uint8_t>(data, dataLength), output);
    }
    else if (strcmp(argv[0], "w") == 0)
    {
        result = bus->Write(device, span<const uint8_t>(data, dataLength));
    }
    else if (strcmp(argv[0], "r") == 0)
    {
        auto p = atoi(argv[3]);
        result = bus->Read(device, gsl::make_span(output).subspan(0, p));
    }
    else
    {
        GetTerminal().Puts("Unknown mode\n");
        return;
    }

    if (result == I2CResult::OK)
    {
        GetTerminal().Puts((char*)output);
    }
    else
    {
        GetTerminal().Printf("Error %d\n", num(result));
    }
}
