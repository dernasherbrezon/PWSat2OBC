#include "fs/fs.h"
#include "n25q.h"

using obc::storage::N25QStorage;
using devices::n25q::OperationResult;

N25QStorage::N25QStorage(drivers::spi::ISPIInterface& spi, FileSystem& fs)
    : ExternalFlashDriver(spi),                //
      ExternalFlash("/", ExternalFlashDriver), //
      _fs(fs)
{
}

void N25QStorage::Initialize()
{
    if (OS_RESULT_FAILED(this->ExternalFlash.Mount()))
    {
        return;
    }
}

OSResult N25QStorage::ClearStorage()
{
    return this->_fs.ClearDevice(&this->_fs, this->ExternalFlash.Device());
}

OSResult N25QStorage::Erase()
{
    auto r = this->ExternalFlashDriver.EraseChip();
    switch (r)
    {
        case OperationResult::Success:
            return OSResult::Success;
        case OperationResult::Timeout:
            return OSResult::Timeout;
        case OperationResult::Failure:
        default:
            return OSResult::IOError;
    }
}
