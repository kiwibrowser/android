// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/drivefs/fake_drivefs.h"

#include <utility>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_cros_disks_client.h"

namespace drivefs {
namespace {

class FakeDriveFsMojoConnectionDelegate
    : public drivefs::DriveFsHost::MojoConnectionDelegate {
 public:
  FakeDriveFsMojoConnectionDelegate(
      drivefs::mojom::DriveFsBootstrapPtrInfo bootstrap)
      : bootstrap_(std::move(bootstrap)) {}

  drivefs::mojom::DriveFsBootstrapPtrInfo InitializeMojoConnection() override {
    return std::move(bootstrap_);
  }

  void AcceptMojoConnection(base::ScopedFD handle) override { NOTREACHED(); }

 private:
  drivefs::mojom::DriveFsBootstrapPtrInfo bootstrap_;

  DISALLOW_COPY_AND_ASSIGN(FakeDriveFsMojoConnectionDelegate);
};

std::vector<std::pair<base::RepeatingCallback<AccountId()>,
                      base::WeakPtr<FakeDriveFs>>>&
GetRegisteredFakeDriveFsIntances() {
  static base::NoDestructor<std::vector<std::pair<
      base::RepeatingCallback<AccountId()>, base::WeakPtr<FakeDriveFs>>>>
      registered_fake_drivefs_instances;
  return *registered_fake_drivefs_instances;
}

base::FilePath MaybeMountDriveFs(
    const std::string& source_path,
    const std::vector<std::string>& mount_options) {
  GURL source_url(source_path);
  DCHECK(source_url.is_valid());
  if (source_url.scheme() != "drivefs") {
    return {};
  }
  std::string datadir_suffix;
  for (const auto& option : mount_options) {
    if (base::StartsWith(option, "datadir=", base::CompareCase::SENSITIVE)) {
      auto datadir =
          base::FilePath(base::StringPiece(option).substr(strlen("datadir=")));
      CHECK(datadir.IsAbsolute());
      CHECK(!datadir.ReferencesParent());
      datadir_suffix = datadir.BaseName().value();
      break;
    }
  }
  CHECK(!datadir_suffix.empty());
  for (auto& registration : GetRegisteredFakeDriveFsIntances()) {
    AccountId account_id = registration.first.Run();
    if (registration.second && account_id.HasAccountIdKey() &&
        account_id.GetAccountIdKey() == datadir_suffix) {
      return registration.second->mount_path();
    }
  }
  NOTREACHED() << datadir_suffix;
  return {};
}

}  // namespace

struct FakeDriveFs::FileMetadata {
  std::string mime_type;
  bool pinned = false;
  bool hosted = false;
  std::string original_name;
};

FakeDriveFs::FakeDriveFs(const base::FilePath& mount_path)
    : mount_path_(mount_path),
      binding_(this),
      bootstrap_binding_(this),
      weak_factory_(this) {
  CHECK(mount_path.IsAbsolute());
  CHECK(!mount_path.ReferencesParent());
}

FakeDriveFs::~FakeDriveFs() = default;

void FakeDriveFs::RegisterMountingForAccountId(
    base::RepeatingCallback<AccountId()> account_id_getter) {
  chromeos::DBusThreadManager* dbus_thread_manager =
      chromeos::DBusThreadManager::Get();
  static_cast<chromeos::FakeCrosDisksClient*>(
      dbus_thread_manager->GetCrosDisksClient())
      ->SetCustomMountPointCallback(base::BindRepeating(&MaybeMountDriveFs));

  GetRegisteredFakeDriveFsIntances().emplace_back(std::move(account_id_getter),
                                                  weak_factory_.GetWeakPtr());
}

std::unique_ptr<drivefs::DriveFsHost::MojoConnectionDelegate>
FakeDriveFs::CreateConnectionDelegate() {
  drivefs::mojom::DriveFsBootstrapPtrInfo bootstrap;
  bootstrap_binding_.Bind(mojo::MakeRequest(&bootstrap));
  pending_delegate_request_ = mojo::MakeRequest(&delegate_);
  delegate_->OnMounted();
  return std::make_unique<FakeDriveFsMojoConnectionDelegate>(
      std::move(bootstrap));
}

void FakeDriveFs::SetMetadata(const base::FilePath& path,
                              const std::string& mime_type,
                              const std::string& original_name) {
  auto& stored_metadata = metadata_[path];
  stored_metadata.mime_type = mime_type;
  stored_metadata.original_name = original_name;
  stored_metadata.hosted = (original_name != path.BaseName().value());
}

void FakeDriveFs::Init(drivefs::mojom::DriveFsConfigurationPtr config,
                       drivefs::mojom::DriveFsRequest drive_fs_request,
                       drivefs::mojom::DriveFsDelegatePtr delegate) {
  mojo::FuseInterface(std::move(pending_delegate_request_),
                      delegate.PassInterface());
  binding_.Bind(std::move(drive_fs_request));
}

void FakeDriveFs::GetMetadata(const base::FilePath& path,
                              bool want_thumbnail,
                              GetMetadataCallback callback) {
  base::FilePath absolute_path = mount_path_;
  CHECK(base::FilePath("/").AppendRelativePath(path, &absolute_path));
  base::File::Info info;
  if (!base::GetFileInfo(absolute_path, &info)) {
    std::move(callback).Run(drive::FILE_ERROR_NOT_FOUND, nullptr);
    return;
  }
  auto metadata = drivefs::mojom::FileMetadata::New();
  metadata->size = info.size;
  metadata->modification_time = info.last_modified;

  const auto& stored_metadata = metadata_[path];
  metadata->pinned = stored_metadata.pinned;

  metadata->content_mime_type = stored_metadata.mime_type;
  metadata->type = stored_metadata.hosted
                       ? mojom::FileMetadata::Type::kHosted
                       : info.is_directory
                             ? mojom::FileMetadata::Type::kDirectory
                             : mojom::FileMetadata::Type::kFile;

  base::StringPiece prefix;
  if (stored_metadata.hosted) {
    prefix = "https://document_alternate_link/";
  } else if (info.is_directory) {
    prefix = "https://folder_alternate_link/";
  } else {
    prefix = "https://file_alternate_link/";
  }
  std::string suffix = stored_metadata.original_name.empty()
                           ? path.BaseName().value()
                           : stored_metadata.original_name;
  metadata->alternate_url = GURL(base::StrCat({prefix, suffix})).spec();
  metadata->capabilities = mojom::Capabilities::New();

  std::move(callback).Run(drive::FILE_ERROR_OK, std::move(metadata));
}

void FakeDriveFs::SetPinned(const base::FilePath& path,
                            bool pinned,
                            SetPinnedCallback callback) {
  metadata_[path].pinned = pinned;
  std::move(callback).Run(drive::FILE_ERROR_OK);
}

}  // namespace drivefs
