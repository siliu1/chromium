// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/conflicts/module_info_win.h"

#include <windows.h>

#include <fileapi.h>

#include <memory>
#include <tuple>

#include "base/file_version_info.h"
#include "base/i18n/case_conversion.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"

namespace {

// Using the module path, populates |inspection_result| with information
// available via the file on disk. For example, this includes the description
// and the certificate information.
void PopulateModuleInfoData(const base::FilePath& module_path,
                            ModuleInspectionResult* inspection_result) {
  inspection_result->location = module_path.value();

  std::unique_ptr<FileVersionInfo> file_version_info(
      FileVersionInfo::CreateFileVersionInfo(module_path));
  if (file_version_info) {
    inspection_result->product_name = file_version_info->product_name();
    inspection_result->description = file_version_info->file_description();
    inspection_result->version = file_version_info->product_version();
  }

  GetCertificateInfo(module_path, &(inspection_result->certificate_info));
}

// Returns the long path name given a short path name. A short path name is a
// path that follows the 8.3 convention and has ~x in it. If the path is already
// a long path name, the function returns the current path without modification.
bool ConvertToLongPath(const base::string16& short_path,
                       base::string16* long_path) {
  wchar_t long_path_buf[MAX_PATH];
  DWORD return_value =
      ::GetLongPathName(short_path.c_str(), long_path_buf, MAX_PATH);
  if (return_value != 0 && return_value < MAX_PATH) {
    *long_path = long_path_buf;
    return true;
  }

  return false;
}

}  // namespace

// ModuleInfoKey ---------------------------------------------------------------

ModuleInfoKey::ModuleInfoKey(const base::FilePath& module_path,
                             uint32_t module_size,
                             uint32_t module_time_date_stamp)
    : module_path(module_path),
      module_size(module_size),
      module_time_date_stamp(module_time_date_stamp) {}

bool ModuleInfoKey::operator<(const ModuleInfoKey& mik) const {
  // The key consists of the triplet of
  // (module_path, module_size, module_time_date_stamp).
  // Use the std::tuple lexicographic comparison operator.
  return std::tie(module_path, module_size, module_time_date_stamp) <
         std::tie(mik.module_path, mik.module_size, mik.module_time_date_stamp);
}

// ModuleInspectionResult ------------------------------------------------------

ModuleInspectionResult::ModuleInspectionResult() = default;

ModuleInspectionResult::ModuleInspectionResult(
    ModuleInspectionResult&& other) noexcept = default;

ModuleInspectionResult& ModuleInspectionResult::operator=(
    ModuleInspectionResult&& other) noexcept = default;

ModuleInspectionResult::~ModuleInspectionResult() = default;

// ModuleInfoData --------------------------------------------------------------

ModuleInfoData::ModuleInfoData() : process_types(0), module_properties(0) {}

ModuleInfoData::~ModuleInfoData() = default;

ModuleInfoData::ModuleInfoData(ModuleInfoData&& module_data) noexcept = default;

// -----------------------------------------------------------------------------

ModuleInspectionResult InspectModule(const base::FilePath& module_path) {
  ModuleInspectionResult inspection_result;

  PopulateModuleInfoData(module_path, &inspection_result);
  internal::NormalizeInspectionResult(&inspection_result);

  return inspection_result;
}

std::string GenerateCodeId(const ModuleInfoKey& module_key) {
  return base::StringPrintf("%08X%x", module_key.module_time_date_stamp,
                            module_key.module_size);
}

namespace internal {

void NormalizeInspectionResult(ModuleInspectionResult* inspection_result) {
  base::string16 path = inspection_result->location;
  if (!ConvertToLongPath(path, &inspection_result->location))
    inspection_result->location = path;

  inspection_result->location =
      base::i18n::ToLower(inspection_result->location);

  // Location contains the filename, so the last slash is where the path
  // ends.
  size_t last_slash = inspection_result->location.find_last_of(L"\\");
  if (last_slash != base::string16::npos) {
    inspection_result->basename =
        inspection_result->location.substr(last_slash + 1);
    inspection_result->location =
        inspection_result->location.substr(0, last_slash + 1);
  } else {
    inspection_result->basename = inspection_result->location;
    inspection_result->location.clear();
  }

  // Some version strings use ", " instead ".". Convert those.
  base::ReplaceSubstringsAfterOffset(&inspection_result->version, 0, L", ",
                                     L".");

  // Some version strings have things like (win7_rtm.090713-1255) appended
  // to them. Remove that.
  size_t first_space = inspection_result->version.find_first_of(L" ");
  if (first_space != base::string16::npos)
    inspection_result->version =
        inspection_result->version.substr(0, first_space);
}

}  // namespace internal
