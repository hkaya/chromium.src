// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/api/identity/identity_api.h"

#include <string>
#include <utility>

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/api_unittest.h"
#include "extensions/common/extension_builder.h"
#include "extensions/shell/browser/shell_oauth2_token_service.h"
#include "google_apis/gaia/oauth2_mint_token_flow.h"

namespace extensions {
namespace shell {

// A ShellOAuth2TokenService that does not make network requests.
class MockShellOAuth2TokenService : public ShellOAuth2TokenService {
 public:
  // The service starts with no account id or refresh token.
  MockShellOAuth2TokenService() : ShellOAuth2TokenService(nullptr, "", "") {}
  ~MockShellOAuth2TokenService() override {}

  // OAuth2TokenService:
  scoped_ptr<Request> StartRequest(const std::string& account_id,
                                   const ScopeSet& scopes,
                                   Consumer* consumer) override {
    // Immediately return success.
    consumer->OnGetTokenSuccess(nullptr, "logged-in-user-token", base::Time());
    return nullptr;
  }
};

// A mint token flow that immediately returns a known access token when started.
class MockOAuth2MintTokenFlow : public OAuth2MintTokenFlow {
 public:
  explicit MockOAuth2MintTokenFlow(Delegate* delegate)
      : OAuth2MintTokenFlow(delegate, Parameters()), delegate_(delegate) {}
  ~MockOAuth2MintTokenFlow() override {}

  // OAuth2ApiCallFlow:
  void Start(net::URLRequestContextGetter* context,
             const std::string& access_token) override {
    EXPECT_EQ("logged-in-user-token", access_token);
    delegate_->OnMintTokenSuccess("app-access-token", 12345);
  }

 private:
  // Cached here so OAuth2MintTokenFlow does not have to expose its delegate.
  Delegate* delegate_;
};

class IdentityApiTest : public ApiUnitTest {
 public:
  IdentityApiTest() {}
  ~IdentityApiTest() override {}

  // testing::Test:
  void SetUp() override {
    ApiUnitTest::SetUp();
    // Create an extension with OAuth2 scopes.
    set_extension(
        ExtensionBuilder()
            .SetManifest(
                DictionaryBuilder()
                    .Set("name", "Test")
                    .Set("version", "1.0")
                    .Set("oauth2",
                         DictionaryBuilder()
                             .Set("client_id",
                                  "123456.apps.googleusercontent.com")
                             .Set("scopes",
                                  ListBuilder()
                                      .Append("https://www.googleapis.com/"
                                              "auth/drive")
                                      .Build())
                             .Build())
                    .Build())
            .SetLocation(Manifest::UNPACKED)
            .Build());
  }
};

// Verifies that the getAuthToken function exists and can be called without
// crashing.
TEST_F(IdentityApiTest, GetAuthTokenNoRefreshToken) {
  MockShellOAuth2TokenService token_service;

  // Calling getAuthToken() before a refresh token is available causes an error.
  std::string error =
      RunFunctionAndReturnError(new IdentityGetAuthTokenFunction, "[{}]");
  EXPECT_FALSE(error.empty());
}

// Verifies that getAuthToken() returns an app access token.
TEST_F(IdentityApiTest, GetAuthToken) {
  MockShellOAuth2TokenService token_service;

  // Simulate a refresh token being set.
  token_service.SetRefreshToken("larry@google.com", "refresh-token");

  // RunFunctionAndReturnValue takes ownership.
  IdentityGetAuthTokenFunction* function = new IdentityGetAuthTokenFunction;
  function->SetMintTokenFlowForTesting(new MockOAuth2MintTokenFlow(function));

  // Function succeeds and returns a token (for its callback).
  scoped_ptr<base::Value> result = RunFunctionAndReturnValue(function, "[{}]");
  ASSERT_TRUE(result.get());
  std::string value;
  result->GetAsString(&value);
  EXPECT_NE("logged-in-user-token", value);
  EXPECT_EQ("app-access-token", value);
}

// Verifies that the removeCachedAuthToken function exists and can be called
// without crashing.
TEST_F(IdentityApiTest, RemoveCachedAuthToken) {
  MockShellOAuth2TokenService token_service;

  // Function succeeds and returns nothing (for its callback).
  scoped_ptr<base::Value> result = RunFunctionAndReturnValue(
      new IdentityRemoveCachedAuthTokenFunction, "[{}]");
  EXPECT_FALSE(result.get());
}

}  // namespace shell
}  // namespace extensions
