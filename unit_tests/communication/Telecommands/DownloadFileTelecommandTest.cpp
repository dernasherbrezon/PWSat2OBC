#include <algorithm>
#include <array>
#include <cmath>
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "base/reader.h"
#include "base/writer.h"
#include "fs/fs.h"
#include "mock/FsMock.hpp"
#include "mock/comm.hpp"
#include "obc/telecommands/file_system.hpp"
#include "telecommunication/downlink.h"
#include "telecommunication/telecommand_handling.h"
#include "utils.hpp"

using std::array;
using std::copy;
using std::uint8_t;
using testing::_;
using testing::Invoke;
using testing::Eq;
using testing::Each;
using testing::ElementsAreArray;
using testing::ElementsAre;
using testing::StrEq;
using testing::Return;
using testing::Matches;
using testing::AllOf;
using gsl::span;

using services::fs::File;
using services::fs::FileHandle;
using services::fs::SeekOrigin;
using obc::telecommands::DownloadFileTelecommand;
using telecommunication::downlink::DownlinkFrame;
using telecommunication::downlink::DownlinkAPID;
namespace
{
    template <std::size_t Size> using Buffer = std::array<uint8_t, Size>;

    class DownloadFileTelecommandTest : public testing::Test
    {
      protected:
        template <std::size_t Size> void SendRequest(uint8_t correlationId, const std::string& path, const std::array<uint16_t, Size> seqs)
        {
            Buffer<200> buffer;
            Writer w(buffer);
            w.WriteByte(correlationId);
            w.WriteByte(path.length());
            w.WriteArray(gsl::span<const uint8_t>(reinterpret_cast<const uint8_t*>(path.data()), path.length()));
            w.WriteByte(0);

            for (auto& seq : seqs)
            {
                w.WriteDoubleWordLE(seq);
            }

            _telecommand.Handle(_transmitter, w.Capture());
        }

        testing::NiceMock<TransmitterMock> _transmitter;
        testing::NiceMock<FsMock> _fs;

        obc::telecommands::DownloadFileTelecommand _telecommand{_fs};
    };

    TEST_F(DownloadFileTelecommandTest, ShouldTransferRequestedPartsOfFile)
    {
        const std::string path{"/a/file"};

        std::array<uint8_t, DownlinkFrame::MaxPayloadSize> expectedPayload1;
        std::array<uint8_t, DownlinkFrame::MaxPayloadSize> expectedPayload2;

        expectedPayload1.fill(1);
        expectedPayload2.fill(2);

        expectedPayload1[0] = 0xFF;
        expectedPayload1[1] = static_cast<uint8_t>(DownloadFileTelecommand::ErrorCode::Success);

        expectedPayload2[0] = 0xFF;
        expectedPayload2[1] = static_cast<uint8_t>(DownloadFileTelecommand::ErrorCode::Success);

        EXPECT_CALL(_transmitter, SendFrame(IsDownlinkFrame(Eq(DownlinkAPID::FileSend), Eq(0U), ElementsAreArray(expectedPayload1))))
            .WillOnce(Return(true));
        EXPECT_CALL(_transmitter, SendFrame(IsDownlinkFrame(Eq(DownlinkAPID::FileSend), Eq(1U), ElementsAreArray(expectedPayload2))))
            .WillOnce(Return(true));

        constexpr uint8_t maxFileDataSize = DownlinkFrame::MaxPayloadSize - 2;
        Buffer<3 * maxFileDataSize> file;
        std::fill_n(file.begin(), maxFileDataSize, 1);
        std::fill_n(file.begin() + maxFileDataSize, maxFileDataSize, 2);
        std::fill_n(file.begin() + 2 * maxFileDataSize, maxFileDataSize, 3);

        this->_fs.AddFile(path.c_str(), file);

        this->SendRequest(0xFF, path, std::array<uint16_t, 2>{0x1, 0x0});
    }

    TEST_F(DownloadFileTelecommandTest, ShouldSendSmallerPartThanMaximumIfNoEnoughDataLength)
    {
        const std::string path{"/a/file"};

        EXPECT_CALL(_transmitter, SendFrame(IsDownlinkFrame(_, _, SpanOfSize(22)))).WillOnce(Return(true));

        Buffer<20> file;
        std::fill(file.begin(), file.end(), 1);

        this->_fs.AddFile(path.c_str(), file);

        this->SendRequest(0xFF, path, std::array<uint16_t, 1>{0x0});
    }

    TEST_F(DownloadFileTelecommandTest, ShouldSendErrorFrameForSequenceNumberBeyondFile)
    {
        const std::string path{"/a/file"};

        std::array<uint8_t, 8> expectedPayload;

        expectedPayload[0] = static_cast<uint8_t>(DownloadFileTelecommand::ErrorCode::TooBigSeq);
        std::copy(path.begin(), path.end(), expectedPayload.begin() + 1);

        EXPECT_CALL(_transmitter, SendFrame(IsDownlinkFrame(DownlinkAPID::FileSend, 0x20, 0xFF, ElementsAreArray(expectedPayload))))
            .WillOnce(Return(true));

        Buffer<20> file;
        std::fill(file.begin(), file.end(), 1);

        this->_fs.AddFile(path.c_str(), file);

        this->SendRequest(0xFF, path, std::array<uint16_t, 1>{0x20});
    }

    TEST_F(DownloadFileTelecommandTest, ShouldDownloadLastPartOfMultipartFile)
    {
        const std::string path{"/a/file"};

        std::array<uint8_t, 22> expectedPayload;

        expectedPayload.fill(4);

        expectedPayload[0] = 0xFF;
        expectedPayload[1] = static_cast<uint8_t>(DownloadFileTelecommand::ErrorCode::Success);

        EXPECT_CALL(_transmitter, SendFrame(IsDownlinkFrame(_, _, ElementsAreArray(expectedPayload)))).WillOnce(Return(true));

        constexpr uint8_t maxFileDataSize = DownlinkFrame::MaxPayloadSize - 2;
        Buffer<maxFileDataSize * 3 + 20> file;
        std::fill_n(file.begin(), maxFileDataSize, 1);
        std::fill_n(file.begin() + maxFileDataSize, maxFileDataSize, 2);
        std::fill_n(file.begin() + 2 * maxFileDataSize, maxFileDataSize, 3);
        std::fill_n(file.begin() + 3 * maxFileDataSize, 20, 4);

        this->_fs.AddFile(path.c_str(), file);

        this->SendRequest(0xFF, path, std::array<uint16_t, 1>{0x3});
    }

    TEST_F(DownloadFileTelecommandTest, ShouldSendErrorFrameWhenFileNotFound)
    {
        const std::string path{"/a/file"};

        std::array<uint8_t, 9> expectedPayload;

        expectedPayload[0] = 0xFF;
        expectedPayload[1] = static_cast<uint8_t>(DownloadFileTelecommand::ErrorCode::FileNotFound);
        std::copy(path.begin(), path.end(), expectedPayload.begin() + 2);

        EXPECT_CALL(_transmitter, SendFrame(IsDownlinkFrame(Eq(DownlinkAPID::FileSend), Eq(0U), ElementsAreArray(expectedPayload))))
            .WillOnce(Return(true));

        this->SendRequest(0xFF, path, std::array<uint16_t, 1>{0x3});
    }

    TEST_F(DownloadFileTelecommandTest, ShouldSendErrorFrameWhenPathNotNullTerminated)
    {
        const std::string path{'\x64', '\x64', '\x64'};

        std::array<uint8_t, 3> expectedPayload{0xFF, static_cast<uint8_t>(DownloadFileTelecommand::ErrorCode::MalformedRequest), 0x00};

        EXPECT_CALL(_transmitter, SendFrame(IsDownlinkFrame(Eq(DownlinkAPID::FileSend), Eq(0U), ElementsAreArray(expectedPayload))))
            .WillOnce(Return(true));

        Buffer<200> buffer;
        Writer w(buffer);
        w.WriteByte(0xFF);
        w.WriteByte(2);
        w.WriteArray(gsl::span<const uint8_t>(reinterpret_cast<const uint8_t*>(path.data()), path.length()));
        w.WriteByte(0);
        w.WriteDoubleWordLE(3);
        _telecommand.Handle(_transmitter, w.Capture());
    }

    TEST_F(DownloadFileTelecommandTest, ShouldSendErrorFrameWhenDataTooShort)
    {
        const std::string path{'\x64', '\x64', '\x64'};

        std::array<uint8_t, 3> expectedPayload{0xFF, static_cast<uint8_t>(DownloadFileTelecommand::ErrorCode::MalformedRequest), 0x00};

        EXPECT_CALL(_transmitter, SendFrame(IsDownlinkFrame(Eq(DownlinkAPID::FileSend), Eq(0U), ElementsAreArray(expectedPayload))))
            .WillOnce(Return(true));

        Buffer<200> buffer;
        Writer w(buffer);
        w.WriteByte(0xFF);
        w.WriteByte(0xFF);
        _telecommand.Handle(_transmitter, w.Capture());
    }
}
