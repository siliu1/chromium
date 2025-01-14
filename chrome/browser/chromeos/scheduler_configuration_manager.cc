// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/scheduler_configuration_manager.h"

#include <string>

#include "base/bind.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/debug_daemon_client.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "third_party/cros_system_api/dbus/debugd/dbus-constants.h"

namespace chromeos {

SchedulerConfigurationManager::SchedulerConfigurationManager(
    DebugDaemonClient* debug_daemon_client,
    PrefService* local_state)
    : debug_daemon_client_(debug_daemon_client) {
  observer_.Init(local_state);
  observer_.Add(
      prefs::kSchedulerConfiguration,
      base::BindRepeating(&SchedulerConfigurationManager::OnPrefChange,
                          base::Unretained(this)));
  debug_daemon_client_->WaitForServiceToBeAvailable(
      base::BindOnce(&SchedulerConfigurationManager::OnDebugDaemonReady,
                     weak_ptr_factory_.GetWeakPtr()));
}

SchedulerConfigurationManager::~SchedulerConfigurationManager() {}

// static
void SchedulerConfigurationManager::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kSchedulerConfiguration, std::string());
}

void SchedulerConfigurationManager::OnDebugDaemonReady(bool service_is_ready) {
  if (!service_is_ready) {
    LOG(ERROR) << "Debug daemon unavailable";
    return;
  }

  // Initialize the system.
  debug_daemon_ready_ = true;
  OnPrefChange();
}

void SchedulerConfigurationManager::OnPrefChange() {
  // No point in calling debugd if it isn't ready yet. The ready callback will
  // will call this function again to set the initial configuration.
  if (!debug_daemon_ready_) {
    return;
  }

  std::string config_name;
  PrefService* local_state = observer_.prefs();
  if (local_state->HasPrefPath(prefs::kSchedulerConfiguration)) {
    config_name = local_state->GetString(prefs::kSchedulerConfiguration);
  } else {
    config_name = debugd::scheduler_configuration::kPerformanceScheduler;
  }

  // NB: Also send an update when the config gets reset to let the system pick
  // whatever default. Note that the value we read in this case will be the
  // default specified on pref registration, e.g. empty string.
  debug_daemon_client_->SetSchedulerConfiguration(
      config_name,
      base::BindOnce(&SchedulerConfigurationManager::OnConfigurationSet,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SchedulerConfigurationManager::OnConfigurationSet(bool result) {
  if (!result) {
    LOG(ERROR) << "Failed to update scheduler configuration";
  }
}

}  // namespace chromeos
