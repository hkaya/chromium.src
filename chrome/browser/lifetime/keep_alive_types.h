// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LIFETIME_KEEP_ALIVE_TYPES_H_
#define CHROME_BROWSER_LIFETIME_KEEP_ALIVE_TYPES_H_

#include <ostream>

// Types here are used to register KeepAlives.
// They Give indications about which kind of optimizations are allowed during
// the KeepAlive's lifetime. This allows to have more info about the state of
// the browser to optimize the resource consumption.

// Refers to the what the KeepAlive's lifetime is tied to, to help debugging.
enum class KeepAliveOrigin {
  // c/b
  APP_CONTROLLER,
  BROWSER,
  BROWSER_PROCESS_CHROMEOS,
  SESSION_RESTORE,

  // c/b/background
  BACKGROUND_MODE_MANAGER,
  BACKGROUND_MODE_MANAGER_STARTUP,

  // c/b/chromeos
  LOGIN_DISPLAY_HOST_IMPL,

  // c/b/ui
  APP_LIST_SERVICE_VIEWS,
  APP_LIST_SHOWER,
  CHROME_APP_DELEGATE,
  CHROME_VIEWS_DELEGATE,
  LEAKED_UNINSTALL_VIEW,
  PANEL,
  PANEL_VIEW,
  PROFILE_LOADER,
  USER_MANAGER_VIEW
};

// Restart: Allow Chrome to restart when all the registered KeepAlives allow
// restarts
enum class KeepAliveRestartOption { DISABLED, ENABLED };

#ifndef NDEBUG
std::ostream& operator<<(std::ostream& out, const KeepAliveOrigin& origin);
std::ostream& operator<<(std::ostream& out,
                         const KeepAliveRestartOption& restart);
#endif  // ndef NDEBUG

#endif  // CHROME_BROWSER_LIFETIME_KEEP_ALIVE_TYPES_H_
