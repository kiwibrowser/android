// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_shim/unix_domain_socket_acceptor.h"

#include <utility>

#include "base/logging.h"
#include "base/message_loop/message_loop_current.h"
#include "mojo/public/cpp/platform/socket_utils_posix.h"

namespace apps {

UnixDomainSocketAcceptor::UnixDomainSocketAcceptor(const base::FilePath& path,
                                                   Delegate* delegate)
    : server_listen_connection_watcher_(FROM_HERE),
      named_pipe_(path.value()),
      delegate_(delegate) {
  mojo::NamedPlatformChannel::Options options;
  options.server_name = named_pipe_;
  mojo::NamedPlatformChannel channel(options);
  listen_handle_ = channel.TakeServerEndpoint();
  DCHECK(delegate_);
}

UnixDomainSocketAcceptor::~UnixDomainSocketAcceptor() {
  Close();
}

bool UnixDomainSocketAcceptor::Listen() {
  if (!listen_handle_.is_valid())
    return false;

  // Watch the fd for connections, and turn any connections into
  // active sockets.
  base::MessageLoopCurrentForIO::Get()->WatchFileDescriptor(
      listen_handle_.platform_handle().GetFD().get(), true,
      base::MessagePumpForIO::WATCH_READ, &server_listen_connection_watcher_,
      this);
  return true;
}

// Called by libevent when we can read from the fd without blocking.
void UnixDomainSocketAcceptor::OnFileCanReadWithoutBlocking(int fd) {
  DCHECK_EQ(fd, listen_handle_.platform_handle().GetFD().get());
  base::ScopedFD connection_fd;
  if (!mojo::AcceptSocketConnection(fd, &connection_fd)) {
    Close();
    delegate_->OnListenError();
    return;
  }

  if (!connection_fd.is_valid()) {
    // The accept() failed, but not in such a way that the factory needs to be
    // shut down.
    return;
  }

  delegate_->OnClientConnected(mojo::PlatformChannelEndpoint(
      mojo::PlatformHandle(std::move(connection_fd))));
}

void UnixDomainSocketAcceptor::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED() << "Listen fd should never be writable.";
}

void UnixDomainSocketAcceptor::Close() {
  if (!listen_handle_.is_valid())
    return;
  listen_handle_.reset();
  if (unlink(named_pipe_.c_str()) < 0)
    PLOG(ERROR) << "unlink";

  // Unregister libevent for the listening socket and close it.
  server_listen_connection_watcher_.StopWatchingFileDescriptor();
}

}  // namespace apps
