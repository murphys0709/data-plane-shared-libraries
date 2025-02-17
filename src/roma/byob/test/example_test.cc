// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

#include "absl/synchronization/notification.h"
#include "src/roma/byob/example/example.pb.h"
#include "src/roma/byob/test/example_roma_byob_app_service.h"
#include "src/roma/byob/utility/utils.h"
#include "src/roma/config/function_binding_object_v2.h"

namespace privacy_sandbox::server_common::byob::example::test {

namespace {
using ::privacy_sandbox::server_common::byob::HasClonePermissionsByobWorker;
using ::privacy_sandbox::server_common::byob::Mode;
using ::privacy_sandbox::server_common::byob::example::ByobEchoService;
using ::privacy_sandbox::server_common::byob::example::EchoRequest;
using ::privacy_sandbox::server_common::byob::example::EchoResponse;
using ::testing::StrEq;

const std::filesystem::path kUdfPath = "/udf";
const std::filesystem::path kGoLangBinaryFilename = "example_go_udf";
const std::filesystem::path kCPlusPlusBinaryFilename = "example_cc_udf";

std::string LoadCode(ByobEchoService<>& roma_service,
                     std::filesystem::path file_path,
                     int num_workers = std::thread::hardware_concurrency()) {
  absl::Notification notif;
  absl::Status notif_status;
  absl::StatusOr<std::string> code_id =
      roma_service.Register(file_path, notif, notif_status, num_workers);
  CHECK_OK(code_id);
  CHECK(notif.WaitForNotificationWithTimeout(absl::Minutes(1)));
  CHECK_OK(notif_status);
  return *std::move(code_id);
}

ByobEchoService<> GetRomaService(Mode mode) {
  privacy_sandbox::server_common::byob::Config<> config = {
      .roma_container_name = "roma_server",
  };
  absl::StatusOr<ByobEchoService<>> echo_interface =
      ByobEchoService<>::Create(config, mode);
  CHECK_OK(echo_interface);
  return std::move(*echo_interface);
}

TEST(RomaByobExampleTest, LoadCppBinaryInSandboxMode) {
  ByobEchoService<> roma_service = GetRomaService(Mode::kModeSandbox);

  absl::Notification notif;
  absl::Status notif_status;
  absl::StatusOr<std::string> code_id =
      roma_service.Register(kUdfPath / kCPlusPlusBinaryFilename, notif,
                            notif_status, /*num_workers=*/1);

  EXPECT_TRUE(code_id.ok());
  EXPECT_TRUE(notif.WaitForNotificationWithTimeout(absl::Minutes(1)));
  EXPECT_TRUE(notif_status.ok());
}

TEST(RomaByobExampleTest, LoadCppBinaryInNonSandboxMode) {
  Mode mode = Mode::kModeNoSandbox;
  if (!HasClonePermissionsByobWorker(mode)) {
    GTEST_SKIP() << "HasClonePermissionsByobWorker check returned false";
  }
  ByobEchoService<> roma_service = GetRomaService(mode);
  absl::Notification notif;
  absl::Status notif_status;

  absl::StatusOr<std::string> code_id =
      roma_service.Register(kUdfPath / kCPlusPlusBinaryFilename, notif,
                            notif_status, /*num_workers=*/1);

  EXPECT_TRUE(code_id.ok());
  EXPECT_TRUE(notif.WaitForNotificationWithTimeout(absl::Minutes(1)));
  EXPECT_TRUE(notif_status.ok());
}

TEST(RomaByobExampleTest, LoadGoBinaryInSandboxMode) {
  ByobEchoService<> roma_service = GetRomaService(Mode::kModeSandbox);
  absl::Notification notif;
  absl::Status notif_status;

  absl::StatusOr<std::string> code_id = roma_service.Register(
      kUdfPath / kGoLangBinaryFilename, notif, notif_status, /*num_workers=*/1);

  EXPECT_TRUE(code_id.ok());
  EXPECT_TRUE(notif.WaitForNotificationWithTimeout(absl::Minutes(1)));
  EXPECT_TRUE(notif_status.ok());
}

TEST(RomaByobExampleTest, LoadGoBinaryInNonSandboxMode) {
  Mode mode = Mode::kModeNoSandbox;
  if (!HasClonePermissionsByobWorker(mode)) {
    GTEST_SKIP() << "HasClonePermissionsByobWorker check returned false";
  }
  ByobEchoService<> roma_service = GetRomaService(mode);
  absl::Notification notif;
  absl::Status notif_status;

  absl::StatusOr<std::string> code_id = roma_service.Register(
      kUdfPath / kGoLangBinaryFilename, notif, notif_status, /*num_workers=*/1);

  EXPECT_TRUE(code_id.ok());
  EXPECT_TRUE(notif.WaitForNotificationWithTimeout(absl::Minutes(1)));
  EXPECT_TRUE(notif_status.ok());
}

TEST(RomaByobExampleTest, NotifProcessRequestCppBinary) {
  ByobEchoService<> roma_service = GetRomaService(Mode::kModeSandbox);
  const std::string message = "I am a test Cpp message.";
  const std::string code_token = LoadCode(
      roma_service, kUdfPath / kCPlusPlusBinaryFilename, /*num_workers=*/2);
  EchoRequest request;
  request.set_message(message);
  absl::StatusOr<std::unique_ptr<EchoResponse>> response;
  absl::Notification notif;

  CHECK_OK(roma_service.Echo(notif, std::move(request), response,
                             /*metadata=*/{}, code_token));

  CHECK(notif.WaitForNotificationWithTimeout(absl::Minutes(1)));
  CHECK_OK(response);
  CHECK(*response != nullptr);
  EXPECT_THAT((*response)->message(), StrEq(message));
}

TEST(RomaByobExampleTest, AsyncCallbackProcessRequestCppBinary) {
  ByobEchoService<> roma_service = GetRomaService(Mode::kModeSandbox);
  const std::string message = "I am a test Cpp message.";
  const std::string code_token = LoadCode(
      roma_service, kUdfPath / kCPlusPlusBinaryFilename, /*num_workers=*/2);
  EchoRequest bin_request;
  bin_request.set_message(message);
  absl::Notification notif;
  absl::StatusOr<EchoResponse> bin_response;
  auto callback = [&notif, &bin_response](absl::StatusOr<EchoResponse> resp) {
    bin_response = std::move(resp);
    notif.Notify();
  };

  CHECK_OK(roma_service.Echo(callback, std::move(bin_request),
                             /*metadata=*/{}, code_token));

  ASSERT_TRUE(notif.WaitForNotificationWithTimeout(absl::Minutes(1)));
  CHECK_OK(bin_response);
  EXPECT_THAT(bin_response->message(), StrEq(message));
}

TEST(RomaByobExampleTest, NotifProcessRequestGoBinary) {
  ByobEchoService<> roma_service = GetRomaService(Mode::kModeSandbox);
  const std::string message = "I am a test Go binary message.";
  const std::string code_token = LoadCode(
      roma_service, kUdfPath / kGoLangBinaryFilename, /*num_workers=*/2);
  EchoRequest request;
  request.set_message(message);
  absl::StatusOr<std::unique_ptr<EchoResponse>> response;
  absl::Notification notif;

  CHECK_OK(roma_service.Echo(notif, std::move(request), response,
                             /*metadata=*/{}, code_token));

  CHECK(notif.WaitForNotificationWithTimeout(absl::Minutes(1)));
  CHECK_OK(response);
  CHECK(*response != nullptr);
  EXPECT_THAT((*response)->message(), StrEq(message));
}

TEST(RomaByobExampleTest, AsyncCallbackProcessRequestGoBinary) {
  ByobEchoService<> roma_service = GetRomaService(Mode::kModeSandbox);
  const std::string message = "I am a test Go binary message.";
  const std::string code_token = LoadCode(
      roma_service, kUdfPath / kGoLangBinaryFilename, /*num_workers=*/2);
  EchoRequest bin_request;
  bin_request.set_message(message);
  absl::Notification notif;
  absl::StatusOr<EchoResponse> bin_response;
  auto callback = [&notif, &bin_response](absl::StatusOr<EchoResponse> resp) {
    bin_response = std::move(resp);
    notif.Notify();
  };

  CHECK_OK(roma_service.Echo(callback, std::move(bin_request),
                             /*metadata=*/{}, code_token));

  ASSERT_TRUE(notif.WaitForNotificationWithTimeout(absl::Minutes(1)));
  CHECK_OK(bin_response);
  EXPECT_THAT(bin_response->message(), StrEq(message));
}

}  // namespace
}  // namespace privacy_sandbox::server_common::byob::example::test
