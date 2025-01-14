// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILEAPI_OBFUSCATED_FILE_UTIL_DISK_DELEGATE_H_
#define STORAGE_BROWSER_FILEAPI_OBFUSCATED_FILE_UTIL_DISK_DELEGATE_H_

#include "base/component_export.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "storage/browser/fileapi/native_file_util.h"

namespace storage {

// This delegate performs all ObfuscatedFileUtil tasks that actually touch disk.

// TODO(https://crbug.com/93417): To be driven from abstract class
// |ObfuscatedFileUtilDelegate|.
class COMPONENT_EXPORT(STORAGE_BROWSER) ObfuscatedFileUtilDiskDelegate {
 public:
  ObfuscatedFileUtilDiskDelegate();
  ~ObfuscatedFileUtilDiskDelegate() = default;

  bool DirectoryExists(const base::FilePath& path);
  bool DeleteFileOrDirectory(const base::FilePath& path, bool recursive);
  bool IsLink(const base::FilePath& file_path);
  bool PathExists(const base::FilePath& path);

  NativeFileUtil::CopyOrMoveMode CopyOrMoveModeForDestination(
      const FileSystemURL& dest_url,
      bool copy);
  base::File CreateOrOpen(const base::FilePath& path, int file_flags);
  base::File::Error EnsureFileExists(const base::FilePath& path, bool* created);
  base::File::Error CreateDirectory(const base::FilePath& path,
                                    bool exclusive,
                                    bool recursive);
  base::File::Error GetFileInfo(const base::FilePath& path,
                                base::File::Info* file_info);
  base::File::Error Touch(const base::FilePath& path,
                          const base::Time& last_access_time,
                          const base::Time& last_modified_time);
  base::File::Error Truncate(const base::FilePath& path, int64_t length);
  base::File::Error CopyOrMoveFile(const base::FilePath& src_path,
                                   const base::FilePath& dest_path,
                                   FileSystemOperation::CopyOrMoveOption option,
                                   NativeFileUtil::CopyOrMoveMode mode);
  base::File::Error DeleteFile(const base::FilePath& path);

  DISALLOW_COPY_AND_ASSIGN(ObfuscatedFileUtilDiskDelegate);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILEAPI_OBFUSCATED_FILE_UTIL_DISK_DELEGATE_H_
