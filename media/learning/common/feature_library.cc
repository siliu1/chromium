// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/common/feature_library.h"

namespace media {
namespace learning {

using ValueDescription = LearningTask::ValueDescription;
using Ordering = LearningTask::Ordering;

// static
ValueDescription FeatureLibrary::NetworkType() {
  return ValueDescription({"NetworkType", Ordering::kUnordered});
}

}  // namespace learning
}  // namespace media
