#include "settings.hpp"
#include <array>
#include <type_traits>
#include "base/offset.hpp"
#include "base/reader.h"
#include "base/writer.h"
#include "fm25w/fm25w.hpp"

using std::array;

namespace boot
{
    struct MagicNumberField : public Element<std::uint32_t>
    {
    };

    struct BootSlotsField : public Element<std::uint8_t>
    {
    };

    struct FailsafeBootSlotsField : public Element<std::uint8_t>
    {
    };

    struct BootCounterField : public Element<std::uint32_t>
    {
    };

    struct LastConfirmedBootCounterField : public Element<std::uint32_t>
    {
    };

    using BootFields = Offsets<MagicNumberField, BootSlotsField, FailsafeBootSlotsField, BootCounterField, LastConfirmedBootCounterField>;

    BootSettings::BootSettings(devices::fm25w::IFM25WDriver& fram) : _fram(fram)
    {
    }

    void BootSettings::Initialize()
    {
        this->_sync = System::CreateBinarySemaphore(2);
        System::GiveSemaphore(this->_sync);
    }

    bool BootSettings::Lock(std::chrono::milliseconds timeout)
    {
        return OS_RESULT_SUCCEEDED(System::TakeSemaphore(this->_sync, timeout));
    }

    void BootSettings::Unlock()
    {
        System::GiveSemaphore(this->_sync);
    }

    bool BootSettings::CheckMagicNumber() const
    {
        alignas(4) array<std::uint8_t, 4> buf;
        this->_fram.Read(BootFields::Offset<MagicNumberField>(), buf);

        return Reader(buf).ReadDoubleWordLE() == MagicNumber;
    }

    std::uint8_t BootSettings::BootSlots() const
    {
        if (!this->CheckMagicNumber())
        {
            return DefaultBootSlot;
        }

        alignas(4) std::uint8_t r = 0;

        this->_fram.Read(BootFields::Offset<BootSlotsField>(), {&r, 1});

        return r;
    }

    bool BootSettings::BootSlots(std::uint8_t slots)
    {
        this->_fram.Write(BootFields::Offset<BootSlotsField>(), {&slots, 1});
        return this->BootSlots() == slots;
    }

    std::uint8_t BootSettings::FailsafeBootSlots() const
    {
        if (!this->CheckMagicNumber())
        {
            return DefaultFailsafeBootSlot;
        }

        alignas(4) std::uint8_t r = 0;

        this->_fram.Read(BootFields::Offset<FailsafeBootSlotsField>(), {&r, 1});

        return r;
    }

    bool BootSettings::FailsafeBootSlots(std::uint8_t slots)
    {
        this->_fram.Write(BootFields::Offset<FailsafeBootSlotsField>(), {&slots, 1});
        return this->FailsafeBootSlots() == slots;
    }

    std::uint32_t BootSettings::BootCounter() const
    {
        if (!this->CheckMagicNumber())
        {
            return DefaultBootCounter;
        }

        alignas(4) array<std::uint8_t, 4> buf;
        this->_fram.Read(BootFields::Offset<BootCounterField>(), buf);

        return Reader(buf).ReadDoubleWordLE();
    }

    bool BootSettings::BootCounter(std::uint32_t counter)
    {
        array<std::uint8_t, 4> buf;

        Writer(buf).WriteDoubleWordLE(counter);

        this->_fram.Write(BootFields::Offset<BootCounterField>(), buf);

        return this->BootCounter() == counter;
    }

    void BootSettings::MarkAsValid()
    {
        array<std::uint8_t, 4> buf;

        Writer(buf).WriteDoubleWordLE(MagicNumber);
        this->_fram.Write(BootFields::Offset<MagicNumberField>(), buf);
    }

    std::uint32_t BootSettings::LastConfirmedBootCounter() const
    {
        array<std::uint8_t, 4> buf;
        this->_fram.Read(BootFields::Offset<LastConfirmedBootCounterField>(), buf);

        Reader r(buf);

        return r.ReadDoubleWordLE();
    }

    bool BootSettings::ConfirmBoot()
    {
        auto counter = this->BootCounter();

        array<std::uint8_t, 4> buf;

        Writer w(buf);
        w.WriteDoubleWordLE(counter);

        this->_fram.Write(BootFields::Offset<LastConfirmedBootCounterField>(), buf);

        return this->LastConfirmedBootCounter() == counter;
    }

    void BootSettings::Erase()
    {
        array<std::uint8_t, BootFields::Size> buf;
        buf.fill(0xFF);

        this->_fram.Write(0, buf);
    }

    bool BootSettings::IsValidBootSlot(std::uint8_t mask)
    {
        if (mask == UpperBootSlot)
            return true;

        if (mask == SafeModeBootSlot)
            return true;

        auto selectedSlots = mask & 0b00111111;

        if (__builtin_popcount(selectedSlots) == 3)
            return true;

        return false;
    }
}
