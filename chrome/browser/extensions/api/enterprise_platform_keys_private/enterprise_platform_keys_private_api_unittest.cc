// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_platform_keys_private/enterprise_platform_keys_private_api.h"

#include <string>

#include "base/bind.h"
#include "base/location.h"
#include "base/strings/stringprintf.h"
#include "base/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/browser/chromeos/policy/stub_enterprise_install_attributes.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/attestation/attestation_constants.h"
#include "chromeos/attestation/mock_attestation_flow.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/cryptohome/mock_async_method_caller.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/mock_cryptohome_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/signin/core/browser/signin_manager.h"
#include "extensions/common/test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using testing::_;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;
using testing::WithArgs;

namespace utils = extension_function_test_utils;

namespace extensions {
namespace {

// Certificate errors as reported to the calling extension.
const int kDBusError = 1;
const int kUserRejected = 2;
const int kGetCertificateFailed = 3;
const int kResetRequired = 4;

const char kUserEmail[] = "test@google.com";

// A simple functor to invoke a callback with predefined arguments.
class FakeBoolDBusMethod {
 public:
  FakeBoolDBusMethod(chromeos::DBusMethodCallStatus status, bool value)
      : status_(status),
        value_(value) {}

  void operator() (const chromeos::BoolDBusMethodCallback& callback) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(callback, status_, value_));
  }

 private:
  chromeos::DBusMethodCallStatus status_;
  bool value_;
};

void RegisterKeyCallbackTrue(
    chromeos::attestation::AttestationKeyType key_type,
    const cryptohome::Identification& user_id,
    const std::string& key_name,
    const cryptohome::AsyncMethodCaller::Callback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(callback, true, cryptohome::MOUNT_ERROR_NONE));
}

void RegisterKeyCallbackFalse(
    chromeos::attestation::AttestationKeyType key_type,
    const cryptohome::Identification& user_id,
    const std::string& key_name,
    const cryptohome::AsyncMethodCaller::Callback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(callback, false, cryptohome::MOUNT_ERROR_NONE));
}

void SignChallengeCallbackTrue(
    chromeos::attestation::AttestationKeyType key_type,
    const cryptohome::Identification& user_id,
    const std::string& key_name,
    const std::string& domain,
    const std::string& device_id,
    chromeos::attestation::AttestationChallengeOptions options,
    const std::string& challenge,
    const cryptohome::AsyncMethodCaller::DataCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(callback, true, "response"));
}

void SignChallengeCallbackFalse(
    chromeos::attestation::AttestationKeyType key_type,
    const cryptohome::Identification& user_id,
    const std::string& key_name,
    const std::string& domain,
    const std::string& device_id,
    chromeos::attestation::AttestationChallengeOptions options,
    const std::string& challenge,
    const cryptohome::AsyncMethodCaller::DataCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(callback, false, ""));
}

void GetCertificateCallbackTrue(
    chromeos::attestation::AttestationCertificateProfile certificate_profile,
    const AccountId& account_id,
    const std::string& request_origin,
    bool force_new_key,
    const chromeos::attestation::AttestationFlow::CertificateCallback&
        callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(callback, true, "certificate"));
}

void GetCertificateCallbackFalse(
    chromeos::attestation::AttestationCertificateProfile certificate_profile,
    const AccountId& account_id,
    const std::string& request_origin,
    bool force_new_key,
    const chromeos::attestation::AttestationFlow::CertificateCallback&
        callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(callback, false, ""));
}

class EPKPChallengeKeyTestBase : public BrowserWithTestWindowTest {
 protected:
  EPKPChallengeKeyTestBase()
      : settings_helper_(false),
        extension_(test_util::CreateEmptyExtension()),
        profile_manager_(TestingBrowserProcess::GetGlobal()),
        fake_user_manager_(new chromeos::FakeChromeUserManager),
        user_manager_enabler_(fake_user_manager_) {
    // Set up the default behavior of mocks.
    ON_CALL(mock_cryptohome_client_, TpmAttestationDoesKeyExist(_, _, _, _))
        .WillByDefault(WithArgs<3>(Invoke(FakeBoolDBusMethod(
            chromeos::DBUS_METHOD_CALL_SUCCESS, false))));
    ON_CALL(mock_cryptohome_client_, TpmAttestationIsPrepared(_))
        .WillByDefault(Invoke(FakeBoolDBusMethod(
            chromeos::DBUS_METHOD_CALL_SUCCESS, true)));
    ON_CALL(mock_async_method_caller_, TpmAttestationRegisterKey(_, _, _, _))
        .WillByDefault(Invoke(RegisterKeyCallbackTrue));
    ON_CALL(mock_async_method_caller_,
            TpmAttestationSignEnterpriseChallenge(_, _, _, _, _, _, _, _))
        .WillByDefault(Invoke(SignChallengeCallbackTrue));
    ON_CALL(mock_attestation_flow_, GetCertificate(_, _, _, _, _))
        .WillByDefault(Invoke(GetCertificateCallbackTrue));

    // Set the Enterprise install attributes.
    stub_install_attributes_.SetDomain("google.com");
    stub_install_attributes_.SetRegistrationUser(kUserEmail);
    stub_install_attributes_.SetDeviceId("device_id");
    stub_install_attributes_.SetMode(policy::DEVICE_MODE_ENTERPRISE);

    settings_helper_.ReplaceProvider(chromeos::kDeviceAttestationEnabled);
    settings_helper_.SetBoolean(chromeos::kDeviceAttestationEnabled, true);
  }

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());

    BrowserWithTestWindowTest::SetUp();

    // Set the user preferences.
    prefs_ = browser()->profile()->GetPrefs();
    base::ListValue whitelist;
    whitelist.AppendString(extension_->id());
    prefs_->Set(prefs::kAttestationExtensionWhitelist, whitelist);

    SetAuthenticatedUser();
  }

  // This will be called by BrowserWithTestWindowTest::SetUp();
  TestingProfile* CreateProfile() override {
    fake_user_manager_->AddUserWithAffiliation(
        AccountId::FromUserEmail(kUserEmail), true);
    return profile_manager_.CreateTestingProfile(kUserEmail);
  }

  void DestroyProfile(TestingProfile* profile) override {
    profile_manager_.DeleteTestingProfile(profile->GetProfileUserName());
    // Profile itself will be destroyed later in
    // ProfileManager::ProfileInfo::~ProfileInfo() .
  }

  // Derived classes can override this method to set the required authenticated
  // user in the SigninManager class.
  virtual void SetAuthenticatedUser() {
    SigninManagerFactory::GetForProfile(browser()->profile())->
        SetAuthenticatedAccountInfo("12345", kUserEmail);
  }

  NiceMock<chromeos::MockCryptohomeClient> mock_cryptohome_client_;
  NiceMock<cryptohome::MockAsyncMethodCaller> mock_async_method_caller_;
  NiceMock<chromeos::attestation::MockAttestationFlow> mock_attestation_flow_;
  chromeos::ScopedCrosSettingsTestHelper settings_helper_;
  scoped_refptr<extensions::Extension> extension_;
  policy::StubEnterpriseInstallAttributes stub_install_attributes_;
  TestingProfileManager profile_manager_;
  // fake_user_manager_ is owned by user_manager_enabler_.
  chromeos::FakeChromeUserManager* fake_user_manager_;
  chromeos::ScopedUserManagerEnabler user_manager_enabler_;
  PrefService* prefs_ = nullptr;
};

class EPKPChallengeMachineKeyTest : public EPKPChallengeKeyTestBase {
 protected:
  static const char kArgs[];

  EPKPChallengeMachineKeyTest()
      : impl_(&mock_cryptohome_client_,
              &mock_async_method_caller_,
              &mock_attestation_flow_,
              &stub_install_attributes_),
        func_(new EnterprisePlatformKeysPrivateChallengeMachineKeyFunction(
            &impl_)) {
    func_->set_extension(extension_.get());
  }

  // Returns an error string for the given code.
  std::string GetCertificateError(int error_code) {
    return base::StringPrintf(
        EPKPChallengeMachineKey::kGetCertificateFailedError,
        error_code);
  }

  EPKPChallengeMachineKey impl_;
  scoped_refptr<EnterprisePlatformKeysPrivateChallengeMachineKeyFunction> func_;
};

// Base 64 encoding of 'challenge'.
const char EPKPChallengeMachineKeyTest::kArgs[] = "[\"Y2hhbGxlbmdl\"]";

TEST_F(EPKPChallengeMachineKeyTest, ChallengeBadBase64) {
  EXPECT_EQ(EPKPChallengeKeyBase::kChallengeBadBase64Error,
            utils::RunFunctionAndReturnError(
                func_.get(), "[\"****\"]", browser()));
}

TEST_F(EPKPChallengeMachineKeyTest, NonEnterpriseDevice) {
  stub_install_attributes_.SetRegistrationUser("");

  EXPECT_EQ(EPKPChallengeMachineKey::kNonEnterpriseDeviceError,
            utils::RunFunctionAndReturnError(func_.get(), kArgs, browser()));
}

TEST_F(EPKPChallengeMachineKeyTest, ExtensionNotWhitelisted) {
  base::ListValue empty_whitelist;
  prefs_->Set(prefs::kAttestationExtensionWhitelist, empty_whitelist);

  EXPECT_EQ(EPKPChallengeKeyBase::kExtensionNotWhitelistedError,
            utils::RunFunctionAndReturnError(func_.get(), kArgs, browser()));
}

TEST_F(EPKPChallengeMachineKeyTest, DevicePolicyDisabled) {
  settings_helper_.SetBoolean(chromeos::kDeviceAttestationEnabled, false);

  EXPECT_EQ(EPKPChallengeKeyBase::kDevicePolicyDisabledError,
            utils::RunFunctionAndReturnError(func_.get(), kArgs, browser()));
}

TEST_F(EPKPChallengeMachineKeyTest, DoesKeyExistDbusFailed) {
  EXPECT_CALL(mock_cryptohome_client_, TpmAttestationDoesKeyExist(_, _, _, _))
      .WillRepeatedly(WithArgs<3>(Invoke(FakeBoolDBusMethod(
          chromeos::DBUS_METHOD_CALL_FAILURE, false))));

  EXPECT_EQ(GetCertificateError(kDBusError),
            utils::RunFunctionAndReturnError(func_.get(), kArgs, browser()));
}

TEST_F(EPKPChallengeMachineKeyTest, GetCertificateFailed) {
  EXPECT_CALL(mock_attestation_flow_, GetCertificate(_, _, _, _, _))
      .WillRepeatedly(Invoke(GetCertificateCallbackFalse));

  EXPECT_EQ(GetCertificateError(kGetCertificateFailed),
            utils::RunFunctionAndReturnError(func_.get(), kArgs, browser()));
}

TEST_F(EPKPChallengeMachineKeyTest, SignChallengeFailed) {
  EXPECT_CALL(mock_async_method_caller_,
              TpmAttestationSignEnterpriseChallenge(_, _, _, _, _, _, _, _))
      .WillRepeatedly(Invoke(SignChallengeCallbackFalse));

  EXPECT_EQ(EPKPChallengeKeyBase::kSignChallengeFailedError,
            utils::RunFunctionAndReturnError(func_.get(), kArgs, browser()));
}

TEST_F(EPKPChallengeMachineKeyTest, KeyExists) {
  EXPECT_CALL(mock_cryptohome_client_, TpmAttestationDoesKeyExist(_, _, _, _))
      .WillRepeatedly(WithArgs<3>(Invoke(FakeBoolDBusMethod(
          chromeos::DBUS_METHOD_CALL_SUCCESS, true))));
  // GetCertificate must not be called if the key exists.
  EXPECT_CALL(mock_attestation_flow_, GetCertificate(_, _, _, _, _))
      .Times(0);

  EXPECT_TRUE(utils::RunFunction(func_.get(), kArgs, browser(), utils::NONE));
}

TEST_F(EPKPChallengeMachineKeyTest, Success) {
  // GetCertificate must be called exactly once.
  EXPECT_CALL(mock_attestation_flow_,
              GetCertificate(
                  chromeos::attestation::PROFILE_ENTERPRISE_MACHINE_CERTIFICATE,
                  _, _, _, _))
      .Times(1);
  // SignEnterpriseChallenge must be called exactly once.
  EXPECT_CALL(
      mock_async_method_caller_,
      TpmAttestationSignEnterpriseChallenge(
          chromeos::attestation::KEY_DEVICE, cryptohome::Identification(),
          "attest-ent-machine", "google.com", "device_id", _, "challenge", _))
      .Times(1);

  std::unique_ptr<base::Value> value(utils::RunFunctionAndReturnSingleResult(
      func_.get(), kArgs, browser(), utils::NONE));

  std::string response;
  value->GetAsString(&response);
  EXPECT_EQ("cmVzcG9uc2U=" /* Base64 encoding of 'response' */, response);
}

TEST_F(EPKPChallengeMachineKeyTest, AttestationNotPrepared) {
  EXPECT_CALL(mock_cryptohome_client_, TpmAttestationIsPrepared(_))
      .WillRepeatedly(Invoke(FakeBoolDBusMethod(
          chromeos::DBUS_METHOD_CALL_SUCCESS, false)));

  EXPECT_EQ(GetCertificateError(kResetRequired),
            utils::RunFunctionAndReturnError(func_.get(), kArgs, browser()));
}

TEST_F(EPKPChallengeMachineKeyTest, AttestationPreparedDbusFailed) {
  EXPECT_CALL(mock_cryptohome_client_, TpmAttestationIsPrepared(_))
      .WillRepeatedly(Invoke(FakeBoolDBusMethod(
          chromeos::DBUS_METHOD_CALL_FAILURE, true)));

  EXPECT_EQ(GetCertificateError(kDBusError),
            utils::RunFunctionAndReturnError(func_.get(), kArgs, browser()));
}

class EPKPChallengeUserKeyTest : public EPKPChallengeKeyTestBase {
 protected:
  static const char kArgs[];

  EPKPChallengeUserKeyTest()
      : impl_(&mock_cryptohome_client_,
              &mock_async_method_caller_,
              &mock_attestation_flow_,
              &stub_install_attributes_),
        func_(
            new EnterprisePlatformKeysPrivateChallengeUserKeyFunction(&impl_)) {
    func_->set_extension(extension_.get());
  }

  void SetUp() override {
    EPKPChallengeKeyTestBase::SetUp();

    // Set the user preferences.
    prefs_->SetBoolean(prefs::kAttestationEnabled, true);
  }

  // Returns an error string for the given code.
  std::string GetCertificateError(int error_code) {
    return base::StringPrintf(EPKPChallengeUserKey::kGetCertificateFailedError,
                              error_code);
  }

  EPKPChallengeUserKey impl_;
  scoped_refptr<EnterprisePlatformKeysPrivateChallengeUserKeyFunction> func_;
};

// Base 64 encoding of 'challenge'
const char EPKPChallengeUserKeyTest::kArgs[] = "[\"Y2hhbGxlbmdl\", true]";

TEST_F(EPKPChallengeUserKeyTest, ChallengeBadBase64) {
  EXPECT_EQ(EPKPChallengeKeyBase::kChallengeBadBase64Error,
            utils::RunFunctionAndReturnError(
                func_.get(), "[\"****\", true]", browser()));
}

TEST_F(EPKPChallengeUserKeyTest, UserPolicyDisabled) {
  prefs_->SetBoolean(prefs::kAttestationEnabled, false);

  EXPECT_EQ(EPKPChallengeUserKey::kUserPolicyDisabledError,
            utils::RunFunctionAndReturnError(func_.get(), kArgs, browser()));
}

TEST_F(EPKPChallengeUserKeyTest, ExtensionNotWhitelisted) {
  base::ListValue empty_whitelist;
  prefs_->Set(prefs::kAttestationExtensionWhitelist, empty_whitelist);

  EXPECT_EQ(EPKPChallengeKeyBase::kExtensionNotWhitelistedError,
            utils::RunFunctionAndReturnError(func_.get(), kArgs, browser()));
}

TEST_F(EPKPChallengeUserKeyTest, DevicePolicyDisabled) {
  settings_helper_.SetBoolean(chromeos::kDeviceAttestationEnabled, false);

  EXPECT_EQ(EPKPChallengeKeyBase::kDevicePolicyDisabledError,
            utils::RunFunctionAndReturnError(func_.get(), kArgs, browser()));
}

TEST_F(EPKPChallengeUserKeyTest, DoesKeyExistDbusFailed) {
  EXPECT_CALL(mock_cryptohome_client_, TpmAttestationDoesKeyExist(_, _, _, _))
      .WillRepeatedly(WithArgs<3>(Invoke(FakeBoolDBusMethod(
          chromeos::DBUS_METHOD_CALL_FAILURE, false))));

  EXPECT_EQ(GetCertificateError(kDBusError),
            utils::RunFunctionAndReturnError(func_.get(), kArgs, browser()));
}

TEST_F(EPKPChallengeUserKeyTest, GetCertificateFailed) {
  EXPECT_CALL(mock_attestation_flow_, GetCertificate(_, _, _, _, _))
      .WillRepeatedly(Invoke(GetCertificateCallbackFalse));

  EXPECT_EQ(GetCertificateError(kGetCertificateFailed),
            utils::RunFunctionAndReturnError(func_.get(), kArgs, browser()));
}

TEST_F(EPKPChallengeUserKeyTest, SignChallengeFailed) {
  EXPECT_CALL(mock_async_method_caller_,
              TpmAttestationSignEnterpriseChallenge(_, _, _, _, _, _, _, _))
      .WillRepeatedly(Invoke(SignChallengeCallbackFalse));

  EXPECT_EQ(EPKPChallengeKeyBase::kSignChallengeFailedError,
            utils::RunFunctionAndReturnError(func_.get(), kArgs, browser()));
}

TEST_F(EPKPChallengeUserKeyTest, KeyRegistrationFailed) {
  EXPECT_CALL(mock_async_method_caller_, TpmAttestationRegisterKey(_, _, _, _))
      .WillRepeatedly(Invoke(RegisterKeyCallbackFalse));

  EXPECT_EQ(EPKPChallengeUserKey::kKeyRegistrationFailedError,
            utils::RunFunctionAndReturnError(func_.get(), kArgs, browser()));
}

TEST_F(EPKPChallengeUserKeyTest, KeyExists) {
  EXPECT_CALL(mock_cryptohome_client_, TpmAttestationDoesKeyExist(_, _, _, _))
      .WillRepeatedly(WithArgs<3>(Invoke(FakeBoolDBusMethod(
          chromeos::DBUS_METHOD_CALL_SUCCESS, true))));
  // GetCertificate must not be called if the key exists.
  EXPECT_CALL(mock_attestation_flow_, GetCertificate(_, _, _, _, _))
      .Times(0);

  EXPECT_TRUE(utils::RunFunction(func_.get(), kArgs, browser(), utils::NONE));
}

TEST_F(EPKPChallengeUserKeyTest, KeyNotRegistered) {
  EXPECT_CALL(mock_async_method_caller_, TpmAttestationRegisterKey(_, _, _, _))
      .Times(0);

  EXPECT_TRUE(utils::RunFunction(
      func_.get(), "[\"Y2hhbGxlbmdl\", false]", browser(), utils::NONE));
}

TEST_F(EPKPChallengeUserKeyTest, PersonalDevice) {
  stub_install_attributes_.SetRegistrationUser("");

  // Currently personal devices are not supported.
  EXPECT_EQ(GetCertificateError(kUserRejected),
            utils::RunFunctionAndReturnError(func_.get(), kArgs, browser()));
}

TEST_F(EPKPChallengeUserKeyTest, Success) {
  // GetCertificate must be called exactly once.
  EXPECT_CALL(mock_attestation_flow_,
              GetCertificate(
                  chromeos::attestation::PROFILE_ENTERPRISE_USER_CERTIFICATE,
                  _, _, _, _))
      .Times(1);
  const AccountId account_id = AccountId::FromUserEmail(kUserEmail);
  // SignEnterpriseChallenge must be called exactly once.
  EXPECT_CALL(mock_async_method_caller_,
              TpmAttestationSignEnterpriseChallenge(
                  chromeos::attestation::KEY_USER,
                  cryptohome::Identification(account_id), "attest-ent-user",
                  cryptohome::Identification(account_id).id(), "device_id", _,
                  "challenge", _))
      .Times(1);
  // RegisterKey must be called exactly once.
  EXPECT_CALL(mock_async_method_caller_,
              TpmAttestationRegisterKey(chromeos::attestation::KEY_USER,
                                        cryptohome::Identification(account_id),
                                        "attest-ent-user", _))
      .Times(1);

  std::unique_ptr<base::Value> value(utils::RunFunctionAndReturnSingleResult(
      func_.get(), kArgs, browser(), utils::NONE));

  std::string response;
  value->GetAsString(&response);
  EXPECT_EQ("cmVzcG9uc2U=" /* Base64 encoding of 'response' */, response);
}

TEST_F(EPKPChallengeUserKeyTest, AttestationNotPrepared) {
  EXPECT_CALL(mock_cryptohome_client_, TpmAttestationIsPrepared(_))
      .WillRepeatedly(Invoke(FakeBoolDBusMethod(
          chromeos::DBUS_METHOD_CALL_SUCCESS, false)));

  EXPECT_EQ(GetCertificateError(kResetRequired),
            utils::RunFunctionAndReturnError(func_.get(), kArgs, browser()));
}

TEST_F(EPKPChallengeUserKeyTest, AttestationPreparedDbusFailed) {
  EXPECT_CALL(mock_cryptohome_client_, TpmAttestationIsPrepared(_))
      .WillRepeatedly(Invoke(FakeBoolDBusMethod(
          chromeos::DBUS_METHOD_CALL_FAILURE, true)));

  EXPECT_EQ(GetCertificateError(kDBusError),
            utils::RunFunctionAndReturnError(func_.get(), kArgs, browser()));
}

class EPKPChallengeMachineKeyUnmanagedUserTest
    : public EPKPChallengeMachineKeyTest {
 protected:
  void SetAuthenticatedUser() override {
    SigninManagerFactory::GetForProfile(browser()->profile())
        ->SetAuthenticatedAccountInfo(account_id_.GetGaiaId(),
                                      account_id_.GetUserEmail());
  }

  TestingProfile* CreateProfile() override {
    fake_user_manager_->AddUser(account_id_);
    TestingProfile* profile =
        profile_manager_.CreateTestingProfile(account_id_.GetUserEmail());
    return profile;
  }

  const AccountId account_id_ =
      AccountId::FromUserEmailGaiaId("test@chromium.com", "12345");
};

TEST_F(EPKPChallengeMachineKeyUnmanagedUserTest, UserNotManaged) {
  EXPECT_EQ(EPKPChallengeKeyBase::kUserNotManaged,
            utils::RunFunctionAndReturnError(func_.get(), kArgs, browser()));
}

class EPKPChallengeUserKeyUnmanagedUserTest : public EPKPChallengeUserKeyTest {
 protected:
  void SetAuthenticatedUser() override {
    SigninManagerFactory::GetForProfile(browser()->profile())
        ->SetAuthenticatedAccountInfo(account_id_.GetGaiaId(),
                                      account_id_.GetUserEmail());
  }

  TestingProfile* CreateProfile() override {
    fake_user_manager_->AddUser(account_id_);
    TestingProfile* profile =
        profile_manager_.CreateTestingProfile(account_id_.GetUserEmail());
    return profile;
  }

  const AccountId account_id_ =
      AccountId::FromUserEmailGaiaId("test@chromium.com", "12345");
};

TEST_F(EPKPChallengeUserKeyUnmanagedUserTest, UserNotManaged) {
  EXPECT_EQ(EPKPChallengeKeyBase::kUserNotManaged,
            utils::RunFunctionAndReturnError(func_.get(), kArgs, browser()));
}

}  // namespace
}  // namespace extensions
