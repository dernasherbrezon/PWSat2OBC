#ifndef LIBS_OBC_STATE_PERSISTENT_STORAGE_ACCESS_HPP
#define LIBS_OBC_STATE_PERSISTENT_STORAGE_ACCESS_HPP

#pragma once

#include "fm25w/fm25w.hpp"
#include "obc/IStorageAccess.hpp"
#include "spi/spi.h"

/**
 * @addtogroup StateDef
 * @{
 */
namespace obc
{
    /**
     * @brief This type bridges the persistent state serialization routines and the fram memory that should be used
     * as its intended long term storage.
     */
    class PersistentStorageAccess final : public IStorageAccess
    {
      public:
        /**
         * @brief ctor.
         * @param[in] errors Error counting mechanism
         * @param[in] spis Array of pointers to spi bus controllers that will be used to access 3 memory chips.
         */
        PersistentStorageAccess(error_counter::IErrorCounting& errors, std::array<drivers::spi::ISPIInterface*, 3> spis);

        virtual void Read(std::uint32_t address, gsl::span<std::uint8_t> span) final override;

        virtual void Write(std::uint32_t address, gsl::span<const std::uint8_t> span) final override;

        /**
         * @brief Retrieves driver for single FRAM chip.
         * @tparam DriverIndex Index of the driver from range <0;2>
         * @return Reference to requested storage driver object.
         */
        template <uint8_t DriverIndex> devices::fm25w::FM25WDriver& GetSingleDriver();

        /**
         * @brief Retrieves redundant FRAM driver.
         */
        devices::fm25w::RedundantFM25WDriver& GetRedundantDriver();

      private:
        /** @brief Fram memory controllers. */
        devices::fm25w::FM25WDriver _fm25wDrivers[3];

        /** @brief Redundant Fram memory driver. */
        devices::fm25w::RedundantFM25WDriver _driver;
    };

    template <uint8_t DriverIndex> devices::fm25w::FM25WDriver& PersistentStorageAccess::GetSingleDriver()
    {
        static_assert(DriverIndex >= 0 && DriverIndex < 3, "Driver index must be 0, 1 or 2");
        return _fm25wDrivers[DriverIndex];
    }
}

/** @} */
#endif
