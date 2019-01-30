// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/platform/platform_handle.h"

#include "base/logging.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>

#include "base/win/scoped_handle.h"
#elif defined(OS_FUCHSIA)
#include <lib/fdio/limits.h>
#include <unistd.h>
#include <zircon/status.h>
#include <zircon/syscalls.h>

#include "base/fuchsia/scoped_zx_handle.h"
#elif defined(OS_MACOSX) && !defined(OS_IOS)
#include <mach/mach_vm.h>

#include "base/mac/mach_logging.h"
#include "base/mac/scoped_mach_port.h"
#endif

#if defined(OS_POSIX)
#include <unistd.h>

#include "base/files/scoped_file.h"
#endif

namespace mojo {

namespace {

#if defined(OS_WIN)
base::win::ScopedHandle CloneHandle(const base::win::ScopedHandle& handle) {
  DCHECK(handle.IsValid());

  HANDLE dupe;
  BOOL result = ::DuplicateHandle(::GetCurrentProcess(), handle.Get(),
                                  ::GetCurrentProcess(), &dupe, 0, FALSE,
                                  DUPLICATE_SAME_ACCESS);
  if (!result)
    return base::win::ScopedHandle();
  DCHECK_NE(dupe, INVALID_HANDLE_VALUE);
  return base::win::ScopedHandle(dupe);
}
#elif defined(OS_FUCHSIA)
base::ScopedZxHandle CloneHandle(const base::ScopedZxHandle& handle) {
  DCHECK(handle.is_valid());

  zx_handle_t dupe;
  zx_status_t result =
      zx_handle_duplicate(handle.get(), ZX_RIGHT_SAME_RIGHTS, &dupe);
  DLOG_IF(ERROR, result != ZX_OK) << "zx_duplicate_handle failed: " << result;
  return base::ScopedZxHandle(dupe);
}
#elif defined(OS_MACOSX) && !defined(OS_IOS)
base::mac::ScopedMachSendRight CloneMachPort(
    const base::mac::ScopedMachSendRight& mach_port) {
  DCHECK(mach_port.is_valid());

  kern_return_t kr = mach_port_mod_refs(mach_task_self(), mach_port.get(),
                                        MACH_PORT_RIGHT_SEND, 1);
  if (kr != KERN_SUCCESS) {
    MACH_DLOG(ERROR, kr) << "mach_port_mod_refs";
    return base::mac::ScopedMachSendRight();
  }
  return base::mac::ScopedMachSendRight(mach_port.get());
}
#endif

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
base::ScopedFD CloneFD(const base::ScopedFD& fd) {
  DCHECK(fd.is_valid());
  return base::ScopedFD(dup(fd.get()));
}
#endif

}  // namespace

PlatformHandle::PlatformHandle() = default;

PlatformHandle::PlatformHandle(PlatformHandle&& other) = default;

#if defined(OS_WIN)
PlatformHandle::PlatformHandle(base::win::ScopedHandle handle)
    : type_(Type::kHandle), handle_(std::move(handle)) {}
#elif defined(OS_FUCHSIA)
PlatformHandle::PlatformHandle(base::ScopedZxHandle handle)
    : type_(Type::kHandle), handle_(std::move(handle)) {}
#elif defined(OS_MACOSX) && !defined(OS_IOS)
PlatformHandle::PlatformHandle(base::mac::ScopedMachSendRight mach_port)
    : type_(Type::kMachPort), mach_port_(std::move(mach_port)) {}
#endif

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
PlatformHandle::PlatformHandle(base::ScopedFD fd)
    : type_(Type::kFd), fd_(std::move(fd)) {
#if defined(OS_FUCHSIA)
  DCHECK_LT(fd_.get(), FDIO_MAX_FD);
#endif
}
#endif

PlatformHandle::~PlatformHandle() = default;

PlatformHandle& PlatformHandle::operator=(PlatformHandle&& other) = default;

// static
void PlatformHandle::ToMojoPlatformHandle(PlatformHandle handle,
                                          MojoPlatformHandle* out_handle) {
  DCHECK(out_handle);
  out_handle->struct_size = sizeof(MojoPlatformHandle);
  if (handle.type_ == Type::kNone) {
    out_handle->type = MOJO_PLATFORM_HANDLE_TYPE_INVALID;
    out_handle->value = 0;
    return;
  }

  do {
#if defined(OS_WIN)
    out_handle->type = MOJO_PLATFORM_HANDLE_TYPE_WINDOWS_HANDLE;
    out_handle->value =
        static_cast<uint64_t>(HandleToLong(handle.TakeHandle().Take()));
    break;
#elif defined(OS_FUCHSIA)
    if (handle.is_handle()) {
      out_handle->type = MOJO_PLATFORM_HANDLE_TYPE_FUCHSIA_HANDLE;
      out_handle->value = handle.TakeHandle().release();
      break;
    }
#elif defined(OS_MACOSX) && !defined(OS_IOS)
    if (handle.is_mach_port()) {
      out_handle->type = MOJO_PLATFORM_HANDLE_TYPE_MACH_PORT;
      out_handle->value =
          static_cast<uint64_t>(handle.TakeMachPort().release());
      break;
    }
#endif

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
    DCHECK(handle.is_fd());
    out_handle->type = MOJO_PLATFORM_HANDLE_TYPE_FILE_DESCRIPTOR;
    out_handle->value = static_cast<uint64_t>(handle.TakeFD().release());
#endif
  } while (false);

  // One of the above cases must take ownership of |handle|.
  DCHECK(!handle.is_valid());
}

// static
PlatformHandle PlatformHandle::FromMojoPlatformHandle(
    const MojoPlatformHandle* handle) {
  if (handle->struct_size < sizeof(*handle) ||
      handle->type == MOJO_PLATFORM_HANDLE_TYPE_INVALID) {
    return PlatformHandle();
  }

#if defined(OS_WIN)
  if (handle->type != MOJO_PLATFORM_HANDLE_TYPE_WINDOWS_HANDLE)
    return PlatformHandle();
  return PlatformHandle(
      base::win::ScopedHandle(LongToHandle(static_cast<long>(handle->value))));
#elif defined(OS_FUCHSIA)
  if (handle->type == MOJO_PLATFORM_HANDLE_TYPE_FUCHSIA_HANDLE)
    return PlatformHandle(base::ScopedZxHandle(handle->value));
#elif defined(OS_MACOSX) && !defined(OS_IOS)
  if (handle->type == MOJO_PLATFORM_HANDLE_TYPE_MACH_PORT) {
    return PlatformHandle(base::mac::ScopedMachSendRight(
        static_cast<mach_port_t>(handle->value)));
  }
#endif

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
  if (handle->type != MOJO_PLATFORM_HANDLE_TYPE_FILE_DESCRIPTOR)
    return PlatformHandle();
  return PlatformHandle(base::ScopedFD(static_cast<int>(handle->value)));
#endif
}

void PlatformHandle::reset() {
  type_ = Type::kNone;

#if defined(OS_WIN)
  handle_.Close();
#elif defined(OS_FUCHSIA)
  handle_.reset();
#elif defined(OS_MACOSX) && !defined(OS_IOS)
  mach_port_.reset();
#endif

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
  fd_.reset();
#endif
}

PlatformHandle PlatformHandle::Clone() const {
#if defined(OS_WIN)
  return PlatformHandle(CloneHandle(handle_));
#elif defined(OS_FUCHSIA)
  if (is_valid_handle())
    return PlatformHandle(CloneHandle(handle_));
  return PlatformHandle(CloneFD(fd_));
#elif defined(OS_MACOSX) && !defined(OS_IOS)
  if (is_valid_mach_port())
    return PlatformHandle(CloneMachPort(mach_port_));
  return PlatformHandle(CloneFD(fd_));
#elif defined(OS_POSIX)
  return PlatformHandle(CloneFD(fd_));
#endif
}

}  // namespace mojo
