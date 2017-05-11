#ifndef LIBS_ADCS_EXPERIMENTAL_ADCS_EXPERIMENTAL_DETUMBLING_HPP
#define LIBS_ADCS_EXPERIMENTAL_ADCS_EXPERIMENTAL_DETUMBLING_HPP

#include "DetumblingComputations.hpp"
#include "adcs/adcs.hpp"
#include "base/hertz.hpp"
#include "imtq/imtq.h"

namespace adcs
{
    /**
     * @brief Experimental detumbling.
     */
    class ExperimentalDetumbling final : public IAdcsProcessor
    {
      public:
        /**
         * @brief Ctor.
         *
         * @param[in] imtqDriver_ Low level imtq module driver.
         */
        ExperimentalDetumbling(devices::imtq::IImtqDriver& imtqDriver_);

        virtual OSResult Enable() override final;

        virtual OSResult Disable() override final;

        virtual void Process() override final;

        virtual std::chrono::hertz GetFrequency() const override final;

        /** @brief Algorithm refresh frequency. */
        static constexpr std::chrono::hertz Frequency = std::chrono::hertz{1.0 / DetumblingComputations::Parameters::dt};

      private:
        /** @brief Detumbling computations algorithm. */
        DetumblingComputations detumblingComputations;

        /** @brief Detumbling algorithm state. */
        DetumblingComputations::State detumblingState;

        /** @brief Low level imtq module driver. */
        devices::imtq::IImtqDriver& imtqDriver;
    };
}

#endif /* LIBS_ADCS_EXPERIMENTAL_ADCS_EXPERIMENTAL_DETUMBLING_HPP */
