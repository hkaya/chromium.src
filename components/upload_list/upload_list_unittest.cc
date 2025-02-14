// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/upload_list/upload_list.h"

#include <stddef.h>

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/sequenced_worker_pool_owner.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTestUploadTime[] = "1234567890";
const char kTestUploadId[] = "0123456789abcdef";
const char kTestLocalID[] = "fedcba9876543210";
const char kTestCaptureTime[] = "2345678901";

class UploadListTest : public testing::Test,
                       public UploadList::Delegate {
 public:
  UploadListTest() : worker_pool_owner_(1, "UploadListTest") {}

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  void WriteUploadLog(const std::string& log_data) {
    ASSERT_GT(base::WriteFile(log_path(), log_data.c_str(), log_data.size()),
              0);
  }

  void WaitForUploadList() {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  // UploadList::Delegate:
  void OnUploadListAvailable() override {
    ASSERT_FALSE(quit_closure_.is_null());
    quit_closure_.Run();
  }

  const scoped_refptr<base::SequencedWorkerPool> worker_pool() {
    return worker_pool_owner_.pool();
  }
  base::FilePath log_path() {
    return temp_dir_.path().Append(FILE_PATH_LITERAL("uploads.log"));
  }

 private:
  base::MessageLoop message_loop_;
  base::ScopedTempDir temp_dir_;
  base::SequencedWorkerPoolOwner worker_pool_owner_;
  base::Closure quit_closure_;
};

// These tests test that UploadList can parse a vector of log entry strings of
// various formats to a vector of UploadInfo objects. See the UploadList
// declaration for a description of the log entry string formats.

// Test log entry string with upload time and upload ID.
// This is the format that crash reports are stored in.
TEST_F(UploadListTest, ParseUploadTimeUploadId) {
  std::string test_entry = kTestUploadTime;
  test_entry += ",";
  test_entry.append(kTestUploadId);
  WriteUploadLog(test_entry);

  scoped_refptr<UploadList> upload_list =
      new UploadList(this, log_path(), worker_pool());

  upload_list->LoadUploadListAsynchronously();
  WaitForUploadList();

  std::vector<UploadList::UploadInfo> uploads;
  upload_list->GetUploads(999, &uploads);

  EXPECT_EQ(1u, uploads.size());
  double time_double = uploads[0].upload_time.ToDoubleT();
  EXPECT_STREQ(kTestUploadTime, base::DoubleToString(time_double).c_str());
  EXPECT_STREQ(kTestUploadId, uploads[0].upload_id.c_str());
  EXPECT_STREQ("", uploads[0].local_id.c_str());
  time_double = uploads[0].capture_time.ToDoubleT();
  EXPECT_STREQ("0", base::DoubleToString(time_double).c_str());
}

// Test log entry string with upload time, upload ID and local ID.
// This is the old format that WebRTC logs were stored in.
TEST_F(UploadListTest, ParseUploadTimeUploadIdLocalId) {
  std::string test_entry = kTestUploadTime;
  test_entry += ",";
  test_entry.append(kTestUploadId);
  test_entry += ",";
  test_entry.append(kTestLocalID);
  WriteUploadLog(test_entry);

  scoped_refptr<UploadList> upload_list =
      new UploadList(this, log_path(), worker_pool());

  upload_list->LoadUploadListAsynchronously();
  WaitForUploadList();

  std::vector<UploadList::UploadInfo> uploads;
  upload_list->GetUploads(999, &uploads);

  EXPECT_EQ(1u, uploads.size());
  double time_double = uploads[0].upload_time.ToDoubleT();
  EXPECT_STREQ(kTestUploadTime, base::DoubleToString(time_double).c_str());
  EXPECT_STREQ(kTestUploadId, uploads[0].upload_id.c_str());
  EXPECT_STREQ(kTestLocalID, uploads[0].local_id.c_str());
  time_double = uploads[0].capture_time.ToDoubleT();
  EXPECT_STREQ("0", base::DoubleToString(time_double).c_str());
}

// Test log entry string with upload time, upload ID and capture time.
// This is the format that WebRTC logs that only have been uploaded only are
// stored in.
TEST_F(UploadListTest, ParseUploadTimeUploadIdCaptureTime) {
  std::string test_entry = kTestUploadTime;
  test_entry += ",";
  test_entry.append(kTestUploadId);
  test_entry += ",,";
  test_entry.append(kTestCaptureTime);
  WriteUploadLog(test_entry);

  scoped_refptr<UploadList> upload_list =
      new UploadList(this, log_path(), worker_pool());

  upload_list->LoadUploadListAsynchronously();
  WaitForUploadList();

  std::vector<UploadList::UploadInfo> uploads;
  upload_list->GetUploads(999, &uploads);

  EXPECT_EQ(1u, uploads.size());
  double time_double = uploads[0].upload_time.ToDoubleT();
  EXPECT_STREQ(kTestUploadTime, base::DoubleToString(time_double).c_str());
  EXPECT_STREQ(kTestUploadId, uploads[0].upload_id.c_str());
  EXPECT_STREQ("", uploads[0].local_id.c_str());
  time_double = uploads[0].capture_time.ToDoubleT();
  EXPECT_STREQ(kTestCaptureTime, base::DoubleToString(time_double).c_str());
}

// Test log entry string with local ID and capture time.
// This is the format that WebRTC logs that only are stored locally are stored
// in.
TEST_F(UploadListTest, ParseLocalIdCaptureTime) {
  std::string test_entry = ",,";
  test_entry.append(kTestLocalID);
  test_entry += ",";
  test_entry.append(kTestCaptureTime);
  WriteUploadLog(test_entry);

  scoped_refptr<UploadList> upload_list =
      new UploadList(this, log_path(), worker_pool());

  upload_list->LoadUploadListAsynchronously();
  WaitForUploadList();

  std::vector<UploadList::UploadInfo> uploads;
  upload_list->GetUploads(999, &uploads);

  EXPECT_EQ(1u, uploads.size());
  double time_double = uploads[0].upload_time.ToDoubleT();
  EXPECT_STREQ("0", base::DoubleToString(time_double).c_str());
  EXPECT_STREQ("", uploads[0].upload_id.c_str());
  EXPECT_STREQ(kTestLocalID, uploads[0].local_id.c_str());
  time_double = uploads[0].capture_time.ToDoubleT();
  EXPECT_STREQ(kTestCaptureTime, base::DoubleToString(time_double).c_str());
}

// Test log entry string with upload time, upload ID, local ID and capture
// time.
// This is the format that WebRTC logs that are stored locally and have been
// uploaded are stored in.
TEST_F(UploadListTest, ParseUploadTimeUploadIdLocalIdCaptureTime) {
  std::string test_entry = kTestUploadTime;
  test_entry += ",";
  test_entry.append(kTestUploadId);
  test_entry += ",";
  test_entry.append(kTestLocalID);
  test_entry += ",";
  test_entry.append(kTestCaptureTime);
  WriteUploadLog(test_entry);

  scoped_refptr<UploadList> upload_list =
      new UploadList(this, log_path(), worker_pool());

  upload_list->LoadUploadListAsynchronously();
  WaitForUploadList();

  std::vector<UploadList::UploadInfo> uploads;
  upload_list->GetUploads(999, &uploads);

  EXPECT_EQ(1u, uploads.size());
  double time_double = uploads[0].upload_time.ToDoubleT();
  EXPECT_STREQ(kTestUploadTime, base::DoubleToString(time_double).c_str());
  EXPECT_STREQ(kTestUploadId, uploads[0].upload_id.c_str());
  EXPECT_STREQ(kTestLocalID, uploads[0].local_id.c_str());
  time_double = uploads[0].capture_time.ToDoubleT();
  EXPECT_STREQ(kTestCaptureTime, base::DoubleToString(time_double).c_str());
}

TEST_F(UploadListTest, ParseMultipleEntries) {
  std::string test_entry;
  for (int i = 1; i <= 4; ++i) {
    test_entry.append(kTestUploadTime);
    test_entry += ",";
    test_entry.append(kTestUploadId);
    test_entry += ",";
    test_entry.append(kTestLocalID);
    test_entry += ",";
    test_entry.append(kTestCaptureTime);
    test_entry += "\n";
  }
  WriteUploadLog(test_entry);

  scoped_refptr<UploadList> upload_list =
      new UploadList(this, log_path(), worker_pool());

  upload_list->LoadUploadListAsynchronously();
  WaitForUploadList();

  std::vector<UploadList::UploadInfo> uploads;
  upload_list->GetUploads(999, &uploads);

  EXPECT_EQ(4u, uploads.size());
  for (size_t i = 0; i < uploads.size(); ++i) {
    double time_double = uploads[i].upload_time.ToDoubleT();
    EXPECT_STREQ(kTestUploadTime, base::DoubleToString(time_double).c_str());
    EXPECT_STREQ(kTestUploadId, uploads[i].upload_id.c_str());
    EXPECT_STREQ(kTestLocalID, uploads[i].local_id.c_str());
    time_double = uploads[i].capture_time.ToDoubleT();
    EXPECT_STREQ(kTestCaptureTime, base::DoubleToString(time_double).c_str());
  }
}

// https://crbug.com/597384
TEST_F(UploadListTest, SimultaneousAccess) {
  std::string test_entry = kTestUploadTime;
  test_entry += ",";
  test_entry.append(kTestUploadId);
  test_entry += ",";
  test_entry.append(kTestLocalID);
  test_entry += ",";
  test_entry.append(kTestCaptureTime);
  WriteUploadLog(test_entry);

  scoped_refptr<UploadList> upload_list =
      new UploadList(this, log_path(), worker_pool());

  // Queue up a bunch of loads, waiting only for the first one to complete.
  // Clearing the delegate prevents the QuitClosure from being Run more than
  // once.
  for (int i = 1; i <= 20; ++i) {
    upload_list->LoadUploadListAsynchronously();
  }
  WaitForUploadList();
  upload_list->ClearDelegate();

  // Read the list a few times to try and race one of the loads above.
  for (int i = 1; i <= 4; ++i) {
    std::vector<UploadList::UploadInfo> uploads;
    upload_list->GetUploads(999, &uploads);

    EXPECT_EQ(1u, uploads.size());
    double time_double = uploads[0].upload_time.ToDoubleT();
    EXPECT_STREQ(kTestUploadTime, base::DoubleToString(time_double).c_str());
    EXPECT_STREQ(kTestUploadId, uploads[0].upload_id.c_str());
    EXPECT_STREQ(kTestLocalID, uploads[0].local_id.c_str());
    time_double = uploads[0].capture_time.ToDoubleT();
    EXPECT_STREQ(kTestCaptureTime, base::DoubleToString(time_double).c_str());
  }
}

}  // namespace
