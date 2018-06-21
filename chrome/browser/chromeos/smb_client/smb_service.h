// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SMB_CLIENT_SMB_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_SMB_CLIENT_SMB_SERVICE_H_

#include <memory>
#include <string>

#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/chromeos/file_system_provider/provider_interface.h"
#include "chrome/browser/chromeos/file_system_provider/service.h"
#include "chrome/browser/chromeos/smb_client/smb_errors.h"
#include "chrome/browser/chromeos/smb_client/smb_share_finder.h"
#include "chrome/browser/chromeos/smb_client/temp_file_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/dbus/smb_provider_client.h"
#include "components/keyed_service/core/keyed_service.h"

namespace base {
class FilePath;
}  // namespace base

namespace chromeos {
namespace smb_client {

using file_system_provider::Capabilities;
using file_system_provider::ProvidedFileSystemInfo;
using file_system_provider::ProvidedFileSystemInterface;
using file_system_provider::ProviderId;
using file_system_provider::ProviderInterface;
using file_system_provider::Service;

// Creates and manages an smb file system.
class SmbService : public KeyedService,
                   public base::SupportsWeakPtr<SmbService> {
 public:
  using MountResponse = base::OnceCallback<void(SmbMountResult result)>;

  explicit SmbService(Profile* profile);
  ~SmbService() override;

  // Gets the singleton instance for the |context|.
  static SmbService* Get(content::BrowserContext* context);

  // Starts the process of mounting an SMB file system.
  // Calls SmbProviderClient::Mount().
  void Mount(const file_system_provider::MountOptions& options,
             const base::FilePath& share_path,
             const std::string& username,
             const std::string& password,
             MountResponse callback);

  // Completes the mounting of an SMB file system, passing |options| on to
  // file_system_provider::Service::MountFileSystem(). Passes error status to
  // callback.
  void OnMountResponse(MountResponse callback,
                       const file_system_provider::MountOptions& options,
                       const base::FilePath& share_path,
                       smbprovider::ErrorType error,
                       int32_t mount_id);

  // Gathers the hosts in the network using |share_finder_| and gets the shares
  // for each of the hosts found. |callback| will be called once per host and
  // will contain the URLs to the shares found.
  void GatherSharesInNetwork(GatherSharesResponse callback);

 private:
  // Initializes temp_file_manager_.
  void InitTempFileManager();

  // Calls InitTempFileManager() and calls Mount.
  void InitTempFileManagerAndMount(
      const file_system_provider::MountOptions& options,
      const base::FilePath& share_path,
      const std::string& username,
      const std::string& password,
      MountResponse callback);

  // Calls SmbProviderClient::Mount(). temp_file_manager_ must be initialized
  // before this is called.
  void CallMount(const file_system_provider::MountOptions& options,
                 const base::FilePath& share_path,
                 const std::string& username,
                 const std::string& password,
                 MountResponse callback);

  // Calls file_system_provider::Service::UnmountFileSystem().
  base::File::Error Unmount(
      const std::string& file_system_id,
      file_system_provider::Service::UnmountReason reason) const;

  Service* GetProviderService() const;

  SmbProviderClient* GetSmbProviderClient() const;

  // Attempts to restore any previously mounted shares remembered by the File
  // System Provider.
  void RestoreMounts();

  // Attempts to remount a share with the information in |file_system_info|.
  void Remount(const ProvidedFileSystemInfo& file_system_info);

  // Handles the response from attempting to remount the file system. If
  // remounting fails, this logs and removes the file_system from the volume
  // manager.
  void OnRemountResponse(const std::string& file_system_id,
                         smbprovider::ErrorType error);

  // Sets up SmbService, including setting up Keberos if the user is ChromAD.
  void StartSetup();

  // Completes SmbService setup. Called by StartSetup().
  void CompleteSetup();

  // Handles the response from attempting to setup Kerberos.
  void OnSetupKerberosResponse(bool success);

  // Fires |callback| with |result|.
  void FireMountCallback(MountResponse callback, SmbMountResult result);

  void RecordMountCount() const;

  // Registers host locators for |share_finder_|.
  void RegisterHostLocators();

  // Set up Multicast DNS host locator.
  void SetUpMdnsHostLocator();

  const ProviderId provider_id_;
  Profile* profile_;
  std::unique_ptr<TempFileManager> temp_file_manager_;
  std::unique_ptr<SmbShareFinder> share_finder_;

  DISALLOW_COPY_AND_ASSIGN(SmbService);
};

}  // namespace smb_client
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SMB_CLIENT_SMB_SERVICE_H_
