// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PERMISSION_UMA_UTIL_H_
#define CHROME_BROWSER_PERMISSIONS_PERMISSION_UMA_UTIL_H_

#include "base/logging.h"
#include "base/macros.h"

class GURL;
class Profile;

namespace content {
enum class PermissionType;
}  // namespace content

// Enum for UMA purposes, make sure you update histograms.xml if you add new
// permission actions. Never delete or reorder an entry; only add new entries
// immediately before PERMISSION_NUM
enum PermissionAction {
  GRANTED = 0,
  DENIED = 1,
  DISMISSED = 2,
  IGNORED = 3,
  REVOKED = 4,
  REENABLED = 5,
  REQUESTED = 6,

  // Always keep this at the end.
  PERMISSION_ACTION_NUM,
};

// Provides a convenient way of logging UMA for permission related operations.
class PermissionUmaUtil {
 public:
  static void PermissionRequested(content::PermissionType permission,
                                  const GURL& requesting_origin,
                                  const GURL& embedding_origin,
                                  Profile* profile);
  static void PermissionGranted(content::PermissionType permission,
                                const GURL& requesting_origin);
  static void PermissionDenied(content::PermissionType permission,
                               const GURL& requesting_origin);
  static void PermissionDismissed(content::PermissionType permission,
                                  const GURL& requesting_origin);
  static void PermissionIgnored(content::PermissionType permission,
                                const GURL& requesting_origin);
  static void PermissionRevoked(content::PermissionType permission,
                                const GURL& revoked_origin);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PermissionUmaUtil);
};

#endif  // CHROME_BROWSER_PERMISSIONS_PERMISSION_UMA_UTIL_H_
