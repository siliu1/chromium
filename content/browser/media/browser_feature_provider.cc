// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/browser_feature_provider.h"

#include <utility>

#include "base/threading/sequenced_task_runner_handle.h"
#include "media/learning/common/feature_library.h"
#include "net/base/network_change_notifier.h"

using ::media::learning::FeatureLibrary;
using ::media::learning::FeatureProviderFactoryCB;
using ::media::learning::FeatureValue;
using ::media::learning::FeatureVector;
using ::media::learning::LearningTask;
using ::media::learning::SequenceBoundFeatureProvider;

namespace content {

BrowserFeatureProvider::BrowserFeatureProvider(const LearningTask& task)
    : task_(task) {}

BrowserFeatureProvider::~BrowserFeatureProvider() = default;

// static
SequenceBoundFeatureProvider BrowserFeatureProvider::Create(
    const LearningTask& task) {
  return base::SequenceBound<BrowserFeatureProvider>(
      base::SequencedTaskRunnerHandle::Get(), task);
}

// static
FeatureProviderFactoryCB BrowserFeatureProvider::GetFactoryCB() {
  return base::BindRepeating(&BrowserFeatureProvider::Create);
}

void BrowserFeatureProvider::AddFeatures(FeatureVector features,
                                         FeatureVectorCB cb) {
  const size_t size = task_.feature_descriptions.size();
  if (features.size() < size)
    features.resize(size);

  // Find any features that we're supposed to fill in.
  for (size_t i = 0; i < size; i++) {
    const auto& desc = task_.feature_descriptions[i];
    if (desc.name == FeatureLibrary::NetworkType().name) {
      features[i] = FeatureValue(
          static_cast<int>(net::NetworkChangeNotifier::GetConnectionType()));
    }
  }

  std::move(cb).Run(std::move(features));
}

}  // namespace content
