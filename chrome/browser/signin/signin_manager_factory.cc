// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_manager_factory.h"

#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/account_fetcher_service_factory.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/gaia_cookie_manager_service_factory.h"
#include "chrome/browser/signin/local_auth.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/signin/core/browser/signin_manager.h"

SigninManagerFactory::SigninManagerFactory()
    : BrowserContextKeyedServiceFactory(
        "SigninManager",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(AccountTrackerServiceFactory::GetInstance());
  DependsOn(ChromeSigninClientFactory::GetInstance());
  DependsOn(GaiaCookieManagerServiceFactory::GetInstance());
  DependsOn(ProfileOAuth2TokenServiceFactory::GetInstance());
}

SigninManagerFactory::~SigninManagerFactory() {
}

#if defined(OS_CHROMEOS)
// static
SigninManagerBase* SigninManagerFactory::GetForProfileIfExists(
    Profile* profile) {
  return static_cast<SigninManagerBase*>(
      GetInstance()->GetServiceForBrowserContext(profile, false));
}

const SigninManagerBase* SigninManagerFactory::GetForProfileIfExists(
    const Profile* profile) {
  return static_cast<const SigninManagerBase*>(
      GetInstance()->GetServiceForBrowserContext(
          const_cast<Profile*>(profile), false));
}

// static
SigninManagerBase* SigninManagerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<SigninManagerBase*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

#else
// static
SigninManager* SigninManagerFactory::GetForProfile(Profile* profile) {
  return static_cast<SigninManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SigninManager* SigninManagerFactory::GetForProfileIfExists(Profile* profile) {
  return static_cast<SigninManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, false));
}

// static
const SigninManager* SigninManagerFactory::GetForProfileIfExists(
    const Profile* profile) {
  return static_cast<const SigninManager*>(
      GetInstance()->GetServiceForBrowserContext(
          const_cast<Profile*>(profile), false));
}
#endif

// static
SigninManagerFactory* SigninManagerFactory::GetInstance() {
  return base::Singleton<SigninManagerFactory>::get();
}

void SigninManagerFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  SigninManagerBase::RegisterProfilePrefs(registry);
}

// static
void SigninManagerFactory::RegisterPrefs(PrefRegistrySimple* registry) {
  SigninManagerBase::RegisterPrefs(registry);
}

KeyedService* SigninManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  SigninManagerBase* service = NULL;
  Profile* profile = static_cast<Profile*>(context);
  SigninClient* client =
      ChromeSigninClientFactory::GetInstance()->GetForProfile(profile);
#if defined(OS_CHROMEOS)
  service = new SigninManagerBase(
      client, ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
      AccountTrackerServiceFactory::GetForProfile(profile));
#else
  service = new SigninManager(
      client, ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
      AccountTrackerServiceFactory::GetForProfile(profile),
      GaiaCookieManagerServiceFactory::GetForProfile(profile),
      AccountConsistencyModeManager::GetMethodForProfile(profile));
#endif
  AccountFetcherServiceFactory::GetForProfile(profile);
  service->Initialize(g_browser_process->local_state());
  return service;
}
