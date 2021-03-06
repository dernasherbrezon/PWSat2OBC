#ifndef LIBS_OBC_COMMUNICATION_TELECOMMANDS_INCLUDE_OBC_TELECOMMANDS_FILE_SYSTEM_HPP_
#define LIBS_OBC_COMMUNICATION_TELECOMMANDS_INCLUDE_OBC_TELECOMMANDS_FILE_SYSTEM_HPP_

#include "fs/fs.h"
#include "telecommunication/downlink.h"
#include "telecommunication/telecommand_handling.h"

namespace obc
{
    namespace telecommands
    {
        /**
         * @brief Generic mechanism for sending file part-by-part
         * @ingroup telecommands
         */
        class FileSender final
        {
          public:
            /**
             * @brief Ctor
             * @param path Path to file that will be send
             * @param correlationId Operation correlation id
             * @param transmitter Transmitter
             * @param fs File system
             */
            FileSender(const char* path, uint8_t correlationId, devices::comm::ITransmitter& transmitter, services::fs::IFileSystem& fs);

            /**
             * @brief Checks if requested operation is valid
             * @retval true Everything is ok
             * @retval false Requested file does not exist
             */
            bool IsValid();
            /**
             * @brief Sends single part of file
             * @param seq Sequence number indicating which part of file should be sent
             * @return Operation result
             */
            bool SendPart(std::uint32_t seq);

            /**
             * @brief Calculates max chunk number for file of given size
             * @param fileSize File size
             * @return Max chunk number (0-based index)
             */
            static std::uint32_t MaxChunkNumber(std::uint32_t fileSize);

          private:
            /** @brief Maximum size of file data in a payload */
            static constexpr uint8_t MaxFileDataSize = telecommunication::downlink::DownlinkFrame::MaxPayloadSize - 2;
            /** @brief File to send */
            services::fs::File _file;
            /** @brief Operation correlation id */
            uint8_t _correlationId;
            /** @brief Transmitter */
            devices::comm::ITransmitter& _transmitter;
            /** @brief Opened file size */
            services::fs::FileSize _fileSize;
            /** @brief Last sequence number available for file */
            std::uint32_t _lastSeq;
        };

        /**
         * @brief Download any file
         * @ingroup telecommands
         * @telecommand
         *
         * Command code: 0xAB
         *
         * Parameters:
         *  - 8-bit - Operation correlation id that will be used in response
         *  - 8-bit - Path length
         *  - String - path to file
         *  - 8-bit - Byte '0'
         *  - Array of 32-bit LE - Sequence numbers of parts that will be send
         */
        class DownloadFileTelecommand final : public telecommunication::uplink::Telecommand<0xAB>
        {
          public:
            /**
             * @brief Error codes for downloading files
             */
            enum class ErrorCode : std::uint8_t
            {
                Success = 0x00,
                FileNotFound = 0x01,
                MalformedRequest = 0x02,
                InvalidPath = 0x03,
                TooBigSeq = 0x04
            };

            /**
             * @brief Ctor
             * @param fs File system
             */
            DownloadFileTelecommand(services::fs::IFileSystem& fs);

            virtual void Handle(devices::comm::ITransmitter& transmitter, gsl::span<const std::uint8_t> parameters) override;

          private:
            /** @brief File system */
            services::fs::IFileSystem& _fs;
        };

        /**
         * @brief Remove existing file
         * @ingroup telecommands
         * @telecommand
         *
         * Command code: 0xAC
         *
         * Parameters:
         *  - 8-bit - APID that will be used in response
         *  - 8-bit - Path length
         *  - String - path to file
         *  - 8-bit - Byte '0'
         */
        class RemoveFileTelecommand final : public telecommunication::uplink::Telecommand<0xAC>
        {
          public:
            /**
             * @brief Ctor
             * @param fs File system
             */
            RemoveFileTelecommand(services::fs::IFileSystem& fs);

            virtual void Handle(devices::comm::ITransmitter& transmitter, gsl::span<const std::uint8_t> parameters) override;

          private:
            /** @brief File system */
            services::fs::IFileSystem& _fs;
        };

        /**
         * @brief List files in given path
         * @ingroup telecommands
         * @telecommand
         *
         * Command code: 0xAD
         * Parameters:
         *  - 8-bit - correlation id
         *  - string - path to directory
         *  - 8-bit - byte '0'
         */
        class ListFilesTelecommand : public telecommunication::uplink::Telecommand<0x0F>
        {
          public:
            /**
             * @brief ctor.
             * @param[in] fs Reference to object providing access to filesystem services.
             */
            ListFilesTelecommand(services::fs::IFileSystem& fs);

            virtual void Handle(devices::comm::ITransmitter& transmitter, gsl::span<const std::uint8_t> parameters) override;

          private:
            /** @brief File system */
            services::fs::IFileSystem& _fs;
        };
    }
}

#endif /* LIBS_OBC_COMMUNICATION_TELECOMMANDS_INCLUDE_OBC_TELECOMMANDS_FILE_SYSTEM_HPP_ */
