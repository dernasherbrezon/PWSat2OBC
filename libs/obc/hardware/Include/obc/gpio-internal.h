#ifndef LIBS_OBC_HARDWARE_INCLUDE_OBC_GPIO_INTERNAL_H_
#define LIBS_OBC_HARDWARE_INCLUDE_OBC_GPIO_INTERNAL_H_

#include <cstdint>
#include "base/io_map.h"

namespace obc
{
    namespace gpio
    {
        /**
        * @defgroup obc_hardware_gpio_interal OBC GPIO Support
        * @ingroup obc_hardware_gpio
        *
        * This module contains support structures for checking GPIO pin configuration
        *
        * @{
        */

        /** @brief Conflict detection */
        struct Conflicts
        {
            /** @brief Detect conflicts between pins */
            template <typename Left, typename Right> struct Conflict
            {
                static constexpr auto LeftPort = Left::PinLocation::Port;           //!< Left pin's port
                static constexpr auto LeftPinNumber = Left::PinLocation::PinNumber; //!< Left pin's number

                static constexpr auto RightPort = Right::PinLocation::Port;           //!< Right pin's port
                static constexpr auto RightPinNumber = Right::PinLocation::PinNumber; //!< Right pin's number

                /** @brief true if Left and Right points to the same pin */
                static constexpr bool value = (LeftPort == RightPort) && (LeftPinNumber == RightPinNumber);
            };

            /** @brief Entry point for conflict checking between two locations */
            template <typename Left, typename Right> static constexpr bool Is()
            {
                return Conflict<Left, Right>::value;
            }
        };

        /**
         * @brief Wrapper class checking that pin locations are not overlapping
         */
        template <template <typename...> class Base, typename... Pins> class VerifyPinsUniqueness final : public Base<Pins...>
        {
          private:
            /**
             * @brief Helper class to extract Pins template pack from flattened pins list
             */
            template <typename T> struct Helper;

            template <typename... Items> struct Helper<io_map::PinContainer<Items...>>
            {
                /**
                 * @brief Checks if Pins are not overlapping
                 * @return true if conflict detected
                 */
                template <std::uint8_t Tag, typename Pin, typename... Rest> static constexpr bool ArePinsNotOverlapping()
                {
                    if (HasConflictWith<Pin, Rest...>())
                        return false;

                    return ArePinsNotOverlapping<0, Rest...>();
                }

                /**
                 * @brief Empty list guard
                 * @return Always true
                 */
                template <std::uint8_t Tag> static constexpr bool ArePinsNotOverlapping()
                {
                    return true;
                }

                /**
                 * @brief Checks if Pin has conflict with any other pin location
                 * @return true if conflict detected
                 */
                template <typename Pin, typename Other, typename... Rest> static constexpr bool HasConflictWith()
                {
                    if (Conflicts::template Is<Pin, Other>())
                        return true;

                    return HasConflictWith<Pin, Rest...>();
                }

                /**
                 * @brief Empty list guard
                 * @return Always true
                 */
                template <typename Pin> static constexpr bool HasConflictWith()
                {
                    return false;
                }

                /** @brief Indicates that there is no conflict between pins */
                static constexpr bool HasNoConflict = ArePinsNotOverlapping<0, Items...>();
            };

            static_assert(Helper<typename io_map::Flatten<Pins...>::Result>::HasNoConflict, "Pins must not overlap");
        };

        /** @} */
    }
}

#endif /* LIBS_OBC_HARDWARE_INCLUDE_OBC_GPIO_INTERNAL_H_ */
