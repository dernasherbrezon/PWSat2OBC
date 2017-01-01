#ifndef LIBS_FS_INCLUDE_FS_FS_H_
#define LIBS_FS_INCLUDE_FS_FS_H_

#include <stdbool.h>
#include <cstdint>
#include <gsl/span>
#include "base/os.h"
#include "system.h"

#include "yaffs.hpp"
namespace services
{
    namespace fs
    {
        /**
         * @defgroup fs File system
         *
         * @brief File system API.
         *
         * @{
         */

        /** @brief Directory handle */
        using DirectoryHandle = void*;

        /** @brief File handle */
        using FileHandle = int;

        // yaffs imposes 2GB file size limit
        /** @brief Type that represents file size. */
        using FileSize = std::int32_t;

        /**
         * @brief Type that represents file opening status.
         */
        struct FileOpenResult
        {
            /** Operation status. */
            OSResult Status;
            /** Opened file handle. */
            FileHandle Handle;
        };

        /**
         * @brief Type that represents directory opening status.
         */
        struct DirectoryOpenResult
        {
            /** Operation status. */
            OSResult Status;
            /** Handle to the opened directory. */
            DirectoryHandle Handle;
        };

        /**
         * @brief Type that represents file read/write operation status.
         */
        struct IOResult
        {
            /** Operation status. */
            OSResult Status;
            /** Number of bytes transferred. */
            FileSize BytesTransferred;
        };

        /**
         * @brief Enumerator of all possible file opening modes.
         */
        enum class FileOpen
        {
            /** Open file only if it already exist, fail if it does not exist. */
            Existing = 0,

            /** Opens a file and truncates it so that its size is zero bytes, only if it exists. */
            TruncateExisting = O_TRUNC,

            /** Open file, create a new one if it does not exist. */
            nAlways = O_CREAT,

            /** Always create new file, if it exists truncate its content to zero. */
            CreateAlways = O_CREAT | O_TRUNC,

            /** Creates a new file, only if it does not already exist, fail if it exists. */
            CreateNew = O_CREAT | O_EXCL,

            /** If set, the file offset shall be set to the end of the file prior to each write. */
            Append = O_APPEND,
        };

        /**
         * @brief Enumerator of all possible file access modes.
         */
        enum class FileAccess
        {
            /** Open only for reading. */
            ReadOnly = O_RDONLY,
            /** Open only for writing. */
            WriteOnly = O_WRONLY,
            /** Open for reading and writing. */
            ReadWrite = O_RDWR,
        };

        /**
         * @brief Structure exposing file system API
         */
        struct IFileSystem
        {
            /**
             * @brief Opens file
             * @param[in] path Path to file
             * @param[in] openFlag File opening flags. @see FSFileOpenFlags for details.
             * @param[in] accessMode Requested file access mode. @see FSFileAccessMode for details.
             * @return Operation status. @see FSFileOpenResult for details.
             */
            virtual FileOpenResult Open(const char* path, FileOpen openFlag, FileAccess accessMode) = 0;

            /**
             * @brief Truncates file to given size
             * @param[in] file File handle
             * @param[in] length Desired length
             * @return Operation status.
             */
            virtual OSResult TruncateFile(FileHandle file, FileSize length) = 0;

            /**
             * @brief Writes data to file
             * @param[in] file File handle
             * @param[in] buffer Data buffer
             * @return Operation status. @see FSIOResult for details.
             */
            virtual IOResult Write(FileHandle file, gsl::span<const std::uint8_t> buffer) = 0;

            /**
             * @brief Reads data from file
             * @param[in] file File handle
             * @param[out] buffer Data buffer
             * @return Operation status. @see FSIOResult for details.
             */
            virtual IOResult Read(FileHandle file, gsl::span<std::uint8_t> buffer) = 0;

            /**
             * @brief Closes file
             * @param[in] file File handle
             * @return Operation status.
             */
            virtual OSResult Close(FileHandle file) = 0;

            /**
             * @brief Opens directory
             * @param[in] dirname Directory path
             * @return Directory handle on success. @see FSDirectoryOpenResult for details.
             */
            virtual DirectoryOpenResult OpenDirectory(const char* dirname) = 0;

            /**
             * @brief Reads name of next entry in directory.
             * @param[in] directory Directory handle
             * @return Entry name. NULL if no more entries found.
             */
            virtual char* ReadDirectory(DirectoryHandle directory) = 0;

            /**
             * @brief Closes directory
             * @param[in] directory Directory handle
             * @return Operation status.
             */
            virtual OSResult CloseDirectory(DirectoryHandle directory) = 0;

            /**
             * @brief Formats partition at given mount point. Partition in unmounted before format and mounted again after
             * @param[in] mountPoint Partition mount point
             * @return Operation status
             */
            virtual OSResult Format(const char* mountPoint) = 0;

            /**
             * @brief Creates new directory
             * @param[in] path Path to directory that should be created
             * @return Operation status
             */
            virtual OSResult MakeDirectory(const char* path) = 0;

            /**
             * @brief Checks if path exists
             * @param[in] path Path to check
             * @return true if path exists
             */
            virtual bool Exists(const char* path) = 0;
        };

        /**
         * @brief Wrapper over file handle
         */
        class File : private NotCopyable
        {
          public:
            /**
             * @brief Move constructor
             * @param other Other file (will become invalid)
             */
            File(File&& other) noexcept;
            /**
             * @brief Move operator
             * @param other Other file (will become invalid)
             * @return Reference to this
             */
            File& operator=(File&& other) noexcept;

            /** @brief Desctructor */
            ~File();

            /**
             * @brief Factory method that opens file
             * @param fs File system
             * @param path File path
             * @param mode Open mode
             * @param access Access
             * @return File instance
             */
            static File Open(IFileSystem& fs, const char* path, FileOpen mode, FileAccess access);

            /**
             * @brief Factory method that opens for read
             * @param fs File system
             * @param path File path
             * @param mode Open mode
             * @param access Access
             * @return File instance
             */
            static File OpenRead(
                IFileSystem& fs, const char* path, FileOpen mode = FileOpen::Existing, FileAccess access = FileAccess::ReadOnly);

            /**
             * @brief Factory method that opens for write
             * @param fs File system
             * @param path File path
             * @param mode Open mode
             * @param access Access
             * @return File instance
             */
            static File OpenWrite(
                IFileSystem& fs, const char* path, FileOpen mode = FileOpen::Existing, FileAccess access = FileAccess::WriteOnly);

            /** @brief Implicit cast to bool, true if file opened successfully*/
            inline operator bool();

            /**
             * @brief Reads from file
             * @param buffer Buffer
             * @return Operation result
             */
            IOResult Read(gsl::span<uint8_t> buffer);

            /**
             * @brief Writes to file
             * @param buffer Buffer
             * @return Operation result
             */
            IOResult Write(gsl::span<const uint8_t> buffer);

            /**
             * @brief Truncates file to desired size
             * @param size Desired size
             * @return Operation result
             */
            OSResult Truncate(FileSize size);

          private:
            /**
             * @brief Wraps given file open result
             * @param fs File system interface
             * @param open File open result (may be unsuccessful)
             */
            File(IFileSystem& fs, FileOpenResult open);

            /** @brief File system interface */
            IFileSystem& _fs;
            /** @brief File handle */
            FileHandle _handle;
            /** @brief Flag indicating whether file is opened successfully */
            bool _valid;
        };

        File::operator bool()
        {
            return this->_valid;
        }

        /**
         * @brief This method is responsible for writing contents of the passed buffer to the selected file.
         *
         * If the selected file exists it will be overwritten, if it does not exist it will be created.
         * @param[in] fs FileSystem interface for accessing files.
         * @param[in] file Path to file that should be saved.
         * @param[in] buffer Buffer that contains data that should be saved.
         *
         * @return Operation status. True in case of success, false otherwise.
         */
        bool SaveToFile(IFileSystem& fs, const char* file, gsl::span<const std::uint8_t> buffer);

        /**
         * @brief This procedure is responsible for reading the the contents of the specified file.
         *
         * @param[in] fs FileSystem interface for accessing files.
         * @param[in] filePath Path to the file that contains timer state.
         * @param[in] buffer Buffer that should be updated with the file contents.
         *
         * @return Operation status.
         * @retval true Requested data has been successfully read.
         * @retval false Either selected file was not found, file could not be opened or did not contain
         * requested amount of data.
         */
        bool ReadFromFile(IFileSystem& fs, const char* const filePath, gsl::span<std::uint8_t> buffer);

        /** @} */
    }
}
#endif /* LIBS_FS_INCLUDE_FS_FS_H_ */
