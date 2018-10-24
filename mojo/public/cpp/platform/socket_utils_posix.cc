// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/platform/socket_utils_posix.h"

#include <stddef.h>
#include <sys/socket.h>
#include <unistd.h>

#if !defined(OS_NACL)
#include <sys/uio.h>
#endif

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "build/build_config.h"

namespace mojo {

namespace {

#if !defined(OS_NACL)
bool IsRecoverableError() {
  return errno == ECONNABORTED || errno == EMFILE || errno == ENFILE ||
         errno == ENOMEM || errno == ENOBUFS;
}

bool GetPeerEuid(base::PlatformFile fd, uid_t* peer_euid) {
#if defined(OS_MACOSX) || defined(OS_OPENBSD) || defined(OS_FREEBSD)
  uid_t socket_euid;
  gid_t socket_gid;
  if (getpeereid(fd, &socket_euid, &socket_gid) < 0) {
    PLOG(ERROR) << "getpeereid " << fd;
    return false;
  }
  *peer_euid = socket_euid;
  return true;
#else
  struct ucred cred;
  socklen_t cred_len = sizeof(cred);
  if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cred, &cred_len) < 0) {
    PLOG(ERROR) << "getsockopt " << fd;
    return false;
  }
  if (static_cast<unsigned>(cred_len) < sizeof(cred)) {
    NOTREACHED() << "Truncated ucred from SO_PEERCRED?";
    return false;
  }
  *peer_euid = cred.uid;
  return true;
#endif
}

bool IsPeerAuthorized(base::PlatformFile fd) {
  uid_t peer_euid;
  if (!GetPeerEuid(fd, &peer_euid))
    return false;
  if (peer_euid != geteuid()) {
    DLOG(ERROR) << "Client euid is not authorized";
    return false;
  }
  return true;
}
#endif  // !defined(OS_NACL)

// NOTE: On Linux |SIGPIPE| is suppressed by passing |MSG_NOSIGNAL| to
// |sendmsg()|. On Mac we instead set |SO_NOSIGPIPE| on the socket itself.
#if defined(OS_MACOSX)
constexpr int kSendmsgFlags = 0;
#else
constexpr int kSendmsgFlags = MSG_NOSIGNAL;
#endif

constexpr size_t kMaxSendmsgHandles = 128;

}  // namespace

ssize_t SendmsgWithHandles(base::PlatformFile socket,
                           struct iovec* iov,
                           size_t num_iov,
                           const std::vector<base::ScopedFD>& descriptors) {
  DCHECK(iov);
  DCHECK_GT(num_iov, 0u);
  DCHECK(!descriptors.empty());
  DCHECK_LE(descriptors.size(), kMaxSendmsgHandles);

  char cmsg_buf[CMSG_SPACE(kMaxSendmsgHandles * sizeof(int))];
  struct msghdr msg = {};
  msg.msg_iov = iov;
  msg.msg_iovlen = num_iov;
  msg.msg_control = cmsg_buf;
  msg.msg_controllen = CMSG_LEN(descriptors.size() * sizeof(int));
  struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type = SCM_RIGHTS;
  cmsg->cmsg_len = CMSG_LEN(descriptors.size() * sizeof(int));
  for (size_t i = 0; i < descriptors.size(); ++i) {
    DCHECK_GE(descriptors[i].get(), 0);
    reinterpret_cast<int*>(CMSG_DATA(cmsg))[i] = descriptors[i].get();
  }
  return HANDLE_EINTR(sendmsg(socket, &msg, kSendmsgFlags));
}

bool AcceptSocketConnection(base::PlatformFile server_fd,
                            base::ScopedFD* connection_fd,
                            bool check_peer_user) {
  DCHECK_GE(server_fd, 0);
  connection_fd->reset();
#if defined(OS_NACL)
  NOTREACHED();
  return false;
#else
  base::ScopedFD accepted_handle(HANDLE_EINTR(accept(server_fd, nullptr, 0)));
  if (!accepted_handle.is_valid())
    return IsRecoverableError();
  if (check_peer_user && !IsPeerAuthorized(accepted_handle.get()))
    return true;
  if (!base::SetNonBlocking(accepted_handle.get())) {
    PLOG(ERROR) << "base::SetNonBlocking() failed " << accepted_handle.get();
    return true;
  }

  *connection_fd = std::move(accepted_handle);
  return true;
#endif  // defined(OS_NACL)
}

}  // namespace mojo
