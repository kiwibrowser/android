// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/smb_client/smb_file_system_id.h"

#include <string>

#include "base/files/file_path.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace smb_client {

class SmbFileSystemIdTest : public testing::Test {
 public:
  SmbFileSystemIdTest() = default;
  ~SmbFileSystemIdTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(SmbFileSystemIdTest);
};

TEST_F(SmbFileSystemIdTest, ShouldCreateFileSystemIdCorrectly) {
  const base::FilePath share_path("smb://192.168.0.0/test");
  const int32_t mount_id = 12;

  EXPECT_EQ("12@@smb://192.168.0.0/test",
            CreateFileSystemId(mount_id, share_path, false /* is_kerberos */));
  EXPECT_EQ("12@@smb://192.168.0.0/test@@kerberos_chromad",
            CreateFileSystemId(mount_id, share_path, true /* is_kerberos */));
}

TEST_F(SmbFileSystemIdTest, ShouldParseMountIdCorrectly) {
  const std::string file_system_id_1 = "12@@smb://192.168.0.0/test";
  const std::string file_system_id_2 =
      "13@@smb://192.168.0.1/test@@kerberos_chromad";

  EXPECT_EQ(12, GetMountIdFromFileSystemId(file_system_id_1));
  EXPECT_EQ(13, GetMountIdFromFileSystemId(file_system_id_2));
}

TEST_F(SmbFileSystemIdTest, ShouldParseSharePathCorrectly) {
  const std::string file_system_id_1 = "12@@smb://192.168.0.0/test";
  const base::FilePath expected_share_path_1("smb://192.168.0.0/test");

  const std::string file_system_id_2 =
      "13@@smb://192.168.0.1/test@@kerberos_chromad";
  const base::FilePath expected_share_path_2("smb://192.168.0.1/test");

  EXPECT_EQ(expected_share_path_1,
            GetSharePathFromFileSystemId(file_system_id_1));
  EXPECT_EQ(expected_share_path_2,
            GetSharePathFromFileSystemId(file_system_id_2));
}

TEST_F(SmbFileSystemIdTest, IsKerberosChromadReturnsCorrectly) {
  const std::string kerberos_file_system_id =
      "13@@smb://192.168.0.1/test@@kerberos_chromad";
  const std::string non_kerberos_file_system_id = "12@@smb://192.168.0.0/test";

  EXPECT_TRUE(IsKerberosChromadFileSystemId(kerberos_file_system_id));
  EXPECT_FALSE(IsKerberosChromadFileSystemId(non_kerberos_file_system_id));
}

}  // namespace smb_client
}  // namespace chromeos
