// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_MEDIA_TRANSFER_PROTOCOL_MEDIA_TRANSFER_PROTOCOL_MANAGER_H_
#define SERVICES_DEVICE_MEDIA_TRANSFER_PROTOCOL_MEDIA_TRANSFER_PROTOCOL_MANAGER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "services/device/public/mojom/mtp_file_entry.mojom.h"
#include "services/device/public/mojom/mtp_manager.mojom.h"
#include "services/device/public/mojom/mtp_storage_info.mojom.h"

#if !defined(OS_CHROMEOS)
#error "Only used on ChromeOS"
#endif

namespace device {

// This class handles the interaction with mtpd.
class MediaTransferProtocolManager {
 public:
  // A callback to handle the result of GetStorages().
  // The argument is the returned vector of available MTP storage names.
  using GetStoragesCallback =
      base::OnceCallback<void(const std::vector<std::string>& storages)>;

  // A callback to handle the result of OpenStorage.
  // The first argument is the returned handle.
  // The second argument is true if there was an error.
  using OpenStorageCallback =
      base::Callback<void(const std::string& handle, bool error)>;

  // A callback to handle the result of CloseStorage.
  // The argument is true if there was an error.
  using CloseStorageCallback = base::Callback<void(bool error)>;

  // A callback to handle the result of CreateDirectory.
  // The first argument is true if there was an error.
  using CreateDirectoryCallback = base::Callback<void(bool error)>;

  // A callback to handle the result of ReadFileChunk.
  // The first argument is a string containing the file data.
  // The second argument is true if there was an error.
  using ReadFileCallback =
      base::Callback<void(const std::string& data, bool error)>;

  // A callback to handle the result of RenameObject.
  // The first argument is true if there was an error.
  using RenameObjectCallback = base::Callback<void(bool error)>;

  // A callback to handle the result of CopyFileFromLocal.
  // The first argument is true if there was an error.
  using CopyFileFromLocalCallback = base::Callback<void(bool error)>;

  // A callback to handle the result of DeleteObject.
  // The first argument is true if there was an error.
  using DeleteObjectCallback = base::Callback<void(bool error)>;

  virtual ~MediaTransferProtocolManager() {}

  // This is a combined interface to get existing storages and set a
  // client for incoming storage change events. It is designed to reduce
  // async calls and eliminate a potential race condition between
  // the client being set and storage updates being made.
  virtual void EnumerateStoragesAndSetClient(
      mojom::MtpManagerClientAssociatedPtrInfo client,
      mojom::MtpManager::EnumerateStoragesAndSetClientCallback callback) = 0;

  // Gets all available MTP storages and runs |callback|.
  virtual void GetStorages(GetStoragesCallback callback) const = 0;

  // Gets the metadata for |storage_name| and runs |callback|.
  virtual void GetStorageInfo(
      const std::string& storage_name,
      mojom::MtpManager::GetStorageInfoCallback callback) const = 0;

  // Read the metadata of |storage_name| from device and runs |callback|.
  virtual void GetStorageInfoFromDevice(
      const std::string& storage_name,
      mojom::MtpManager::GetStorageInfoFromDeviceCallback callback) = 0;

  // Opens |storage_name| in |mode| and runs |callback|.
  virtual void OpenStorage(const std::string& storage_name,
                           const std::string& mode,
                           const OpenStorageCallback& callback) = 0;

  // Close |storage_handle| and runs |callback|.
  virtual void CloseStorage(const std::string& storage_handle,
                            const CloseStorageCallback& callback) = 0;

  // Creates |directory_name| in |parent_id|.
  virtual void CreateDirectory(const std::string& storage_handle,
                               uint32_t parent_id,
                               const std::string& directory_name,
                               const CreateDirectoryCallback& callback) = 0;

  // Reads IDs of directory entries from |file_id| on |storage_handle| and runs
  // |callback|.
  virtual void ReadDirectoryEntryIds(
      const std::string& storage_handle,
      uint32_t file_id,
      mojom::MtpManager::ReadDirectoryEntryIdsCallback callback) = 0;

  // Reads file data from |file_id| on |storage_handle| and runs |callback|.
  // Reads |count| bytes of data starting at |offset|.
  virtual void ReadFileChunk(const std::string& storage_handle,
                             uint32_t file_id,
                             uint32_t offset,
                             uint32_t count,
                             const ReadFileCallback& callback) = 0;

  // Gets the metadata for files on |storage_handle| with |file_ids|.
  // Runs |callback| with the results.
  // Use mojom::MtpManager::GetFileInfoCallback directly to get prepared for
  // future merge.
  virtual void GetFileInfo(const std::string& storage_handle,
                           const std::vector<uint32_t>& file_ids,
                           mojom::MtpManager::GetFileInfoCallback callback) = 0;

  // Renames |object_id| to |new_name|.
  virtual void RenameObject(const std::string& storage_handle,
                            uint32_t object_id,
                            const std::string& new_name,
                            const RenameObjectCallback& callback) = 0;

  // Copies the file from |source_file_descriptor| to |file_name| on
  // |parent_id|.
  virtual void CopyFileFromLocal(const std::string& storage_handle,
                                 const int source_file_descriptor,
                                 uint32_t parent_id,
                                 const std::string& file_name,
                                 const CopyFileFromLocalCallback& callback) = 0;

  // Deletes |object_id|.
  virtual void DeleteObject(const std::string& storage_handle,
                            uint32_t object_id,
                            const DeleteObjectCallback& callback) = 0;

  // Creates and returns the global MediaTransferProtocolManager instance.
  static std::unique_ptr<MediaTransferProtocolManager> Initialize();
};

}  // namespace device

#endif  // SERVICES_DEVICE_MEDIA_TRANSFER_PROTOCOL_MEDIA_TRANSFER_PROTOCOL_MANAGER_H_
