#ifndef LIBS_FS_INCLUDE_FS_FS_H_
#define LIBS_FS_INCLUDE_FS_FS_H_

#include <stdbool.h>
#include <cstdint>
#include <gsl/span>
#include "base/os.h"
#include "system.h"

#include "yaffs.hpp"

/**
 * @defgroup fs File system
 *
 * @brief File system API.
 *
 * @{
 */

/** @brief Directory handle */
using FSDirectoryHandle = void*;

/** @brief File handle */
using FSFileHandle = int;

// yaffs imposes 2GB file size limit
/** @brief Type that represents file size. */
using FSFileSize = std::int32_t;

/**
 * @brief Type that represents file opening status.
 */
struct FSFileOpenResult
{
    /** Operation status. */
    OSResult Status;
    /** Opened file handle. */
    FSFileHandle Handle;
};

/**
 * @brief Type that represents directory opening status.
 */
struct FSDirectoryOpenResult
{
    /** Operation status. */
    OSResult Status;
    /** Handle to the opened directory. */
    FSDirectoryHandle Handle;
};

/**
 * @brief Type that represents file read/write operation status.
 */
struct FSIOResult
{
    /** Operation status. */
    OSResult Status;
    /** Number of bytes transferred. */
    FSFileSize BytesTransferred;
};

/**
 * @brief Enumerator of all possible file opening modes.
 */
enum class FSFileOpen
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
enum class FSFileAccess
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
struct FileSystem
{
    /**
     * @brief Opens file
     * @param[in] path Path to file
     * @param[in] openFlag File opening flags. @see FSFileOpenFlags for details.
     * @param[in] accessMode Requested file access mode. @see FSFileAccessMode for details.
     * @return Operation status. @see FSFileOpenResult for details.
     */
    virtual FSFileOpenResult Open(const char* path, FSFileOpen openFlag, FSFileAccess accessMode) = 0;

    /**
     * @brief Truncates file to given size
     * @param[in] file File handle
     * @param[in] length Desired length
     * @return Operation status.
     */
    virtual OSResult TruncateFile(FSFileHandle file, FSFileSize length) = 0;

    /**
     * @brief Writes data to file
     * @param[in] file File handle
     * @param[in] buffer Data buffer
     * @return Operation status. @see FSIOResult for details.
     */
    virtual FSIOResult Write(FSFileHandle file, gsl::span<const std::uint8_t> buffer) = 0;

    /**
     * @brief Reads data from file
     * @param[in] file File handle
     * @param[out] buffer Data buffer
     * @return Operation status. @see FSIOResult for details.
     */
    virtual FSIOResult Read(FSFileHandle file, gsl::span<std::uint8_t> buffer) = 0;

    /**
     * @brief Closes file
     * @param[in] file File handle
     * @return Operation status.
     */
    virtual OSResult Close(FSFileHandle file) = 0;

    /**
     * @brief Opens directory
     * @param[in] dirname Directory path
     * @return Directory handle on success. @see FSDirectoryOpenResult for details.
     */
    virtual FSDirectoryOpenResult OpenDirectory(const char* dirname) = 0;

    /**
     * @brief Reads name of next entry in directory.
     * @param[in] directory Directory handle
     * @return Entry name. NULL if no more entries found.
     */
    virtual char* ReadDirectory(FSDirectoryHandle directory) = 0;

    /**
     * @brief Closes directory
     * @param[in] directory Directory handle
     * @return Operation status.
     */
    virtual OSResult CloseDirectory(FSDirectoryHandle directory) = 0;

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
 * @brief This method is responsible for writing contents of the passed buffer to the selected file.
 *
 * If the selected file exists it will be overwritten, if it does not exist it will be created.
 * @param[in] fs FileSystem interface for accessing files.
 * @param[in] file Path to file that should be saved.
 * @param[in] buffer Buffer that contains data that should be saved.
 *
 * @return Operation status. True in case of success, false otherwise.
 */
bool FileSystemSaveToFile(FileSystem& fs, const char* file, gsl::span<const std::uint8_t> buffer);

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
bool FileSystemReadFile(FileSystem& fs, const char* const filePath, gsl::span<std::uint8_t> buffer);

/** @} */

#endif /* LIBS_FS_INCLUDE_FS_FS_H_ */
