// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_SERIALIZED_HANDLES_H_
#define PPAPI_PROXY_SERIALIZED_HANDLES_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/atomicops.h"
#include "base/logging.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory.h"
#include "build/build_config.h"
#include "ipc/ipc_platform_file.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/proxy/ppapi_proxy_export.h"

namespace base {
class Pickle;
}

namespace ppapi {
namespace proxy {

// SerializedHandle is a unified structure for holding a handle (e.g., a shared
// memory handle, socket descriptor, etc). This is useful for passing handles in
// resource messages and also makes it easier to translate handles in
// NaClIPCAdapter for use in NaCl.
class PPAPI_PROXY_EXPORT SerializedHandle {
 public:
  // TODO(https://crbug.com/845985): Remove SHARED_MEMORY type after all clients
  // will be converted to the SHARED_MEMORY_REGION.
  enum Type { INVALID, SHARED_MEMORY, SHARED_MEMORY_REGION, SOCKET, FILE };
  // Header contains the fields that we send in IPC messages, apart from the
  // actual handle. See comments on the SerializedHandle fields below.
  struct Header {
    Header() : type(INVALID), size(0), open_flags(0) {}
    Header(Type type_arg,
           uint32_t size_arg,
           int32_t open_flags_arg,
           PP_Resource file_io_arg)
        : type(type_arg),
          size(size_arg),
          open_flags(open_flags_arg),
          file_io(file_io_arg) {}

    Type type;
    uint32_t size;
    int32_t open_flags;
    PP_Resource file_io;
  };

  SerializedHandle();
  // Move operations are allowed.
  SerializedHandle(SerializedHandle&&);
  SerializedHandle& operator=(SerializedHandle&&);
  // Create an invalid handle of the given type.
  explicit SerializedHandle(Type type);

  // Create a shared memory handle.
  SerializedHandle(const base::SharedMemoryHandle& handle, uint32_t size);

  // Create a shared memory region handle.
  explicit SerializedHandle(base::subtle::PlatformSharedMemoryRegion region);

  // Create a socket or file handle.
  SerializedHandle(const Type type,
                   const IPC::PlatformFileForTransit& descriptor);

  Type type() const { return type_; }
  bool is_shmem() const { return type_ == SHARED_MEMORY; }
  bool is_shmem_region() const { return type_ == SHARED_MEMORY_REGION; }
  bool is_socket() const { return type_ == SOCKET; }
  bool is_file() const { return type_ == FILE; }
  const base::SharedMemoryHandle& shmem() const {
    DCHECK(is_shmem());
    return shm_handle_;
  }
  uint32_t size() const {
    DCHECK(is_shmem());
    return size_;
  }
  const base::subtle::PlatformSharedMemoryRegion& shmem_region() const {
    DCHECK(is_shmem_region());
    return shm_region_;
  }
  base::subtle::PlatformSharedMemoryRegion TakeSharedMemoryRegion() {
    DCHECK(is_shmem_region());
    return std::move(shm_region_);
  }
  const IPC::PlatformFileForTransit& descriptor() const {
    DCHECK(is_socket() || is_file());
    return descriptor_;
  }
  int32_t open_flags() const { return open_flags_; }
  PP_Resource file_io() const {
    return file_io_;
  }
  void set_shmem(const base::SharedMemoryHandle& handle, uint32_t size) {
    type_ = SHARED_MEMORY;
    shm_handle_ = handle;
    size_ = size;

    descriptor_ = IPC::InvalidPlatformFileForTransit();
    shm_region_ = base::subtle::PlatformSharedMemoryRegion();
  }
  void set_shmem_region(base::subtle::PlatformSharedMemoryRegion region) {
    type_ = SHARED_MEMORY_REGION;
    shm_region_ = std::move(region);
    // Writable regions are not supported.
    DCHECK_NE(shm_region_.GetMode(),
              base::subtle::PlatformSharedMemoryRegion::Mode::kWritable);

    descriptor_ = IPC::InvalidPlatformFileForTransit();
    shm_handle_ = base::SharedMemoryHandle();
    size_ = 0;
  }
  void set_socket(const IPC::PlatformFileForTransit& socket) {
    type_ = SOCKET;
    descriptor_ = socket;

    shm_region_ = base::subtle::PlatformSharedMemoryRegion();
    shm_handle_ = base::SharedMemoryHandle();
    size_ = 0;
  }
  void set_file_handle(const IPC::PlatformFileForTransit& descriptor,
                       int32_t open_flags,
                       PP_Resource file_io) {
    type_ = FILE;

    descriptor_ = descriptor;
    shm_region_ = base::subtle::PlatformSharedMemoryRegion();
    shm_handle_ = base::SharedMemoryHandle();
    size_ = 0;
    open_flags_ = open_flags;
    file_io_ = file_io;
  }
  void set_null() {
    type_ = INVALID;

    shm_handle_ = base::SharedMemoryHandle();
    shm_region_ = base::subtle::PlatformSharedMemoryRegion();
    size_ = 0;
    descriptor_ = IPC::InvalidPlatformFileForTransit();
  }
  void set_null_shmem() { set_shmem(base::SharedMemoryHandle(), 0); }
  void set_null_socket() {
    set_socket(IPC::InvalidPlatformFileForTransit());
  }
  void set_null_file_handle() {
    set_file_handle(IPC::InvalidPlatformFileForTransit(), 0, 0);
  }
  bool IsHandleValid() const;

  Header header() const {
    return Header(type_, size_, open_flags_, file_io_);
  }

  // Closes the handle and sets it to invalid.
  void Close();

  // Write/Read a Header, which contains all the data except the handle. This
  // allows us to write the handle in a platform-specific way, as is necessary
  // in NaClIPCAdapter to share handles with NaCl from Windows.
  static void WriteHeader(const Header& hdr, base::Pickle* pickle);
  static bool ReadHeader(base::PickleIterator* iter, Header* hdr);

 private:
  // The kind of handle we're holding.
  Type type_;

  // We hold more members than we really need; we can't easily use a union,
  // because we hold non-POD types. But these types are pretty light-weight. If
  // we add more complex things later, we should come up with a more memory-
  // efficient strategy.
  // These are valid if type == SHARED_MEMORY.
  base::SharedMemoryHandle shm_handle_;
  uint32_t size_;

  // This is valid if type == SHARED_MEMORY_REGION.
  base::subtle::PlatformSharedMemoryRegion shm_region_;

  // This is valid if type == SOCKET || type == FILE.
  IPC::PlatformFileForTransit descriptor_;

  // The following fields are valid if type == FILE.
  int32_t open_flags_;
  // This is non-zero if file writes require quota checking.
  PP_Resource file_io_;

  DISALLOW_COPY_AND_ASSIGN(SerializedHandle);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_SERIALIZED_HANDLES_H_
