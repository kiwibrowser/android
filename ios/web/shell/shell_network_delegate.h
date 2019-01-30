// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_SHELL_SHELL_NETWORK_DELEGATE_H_
#define IOS_WEB_SHELL_SHELL_NETWORK_DELEGATE_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "net/base/network_delegate_impl.h"

namespace web {

class ShellNetworkDelegate : public net::NetworkDelegateImpl {
 public:
  ShellNetworkDelegate();
  ~ShellNetworkDelegate() override;

 private:
  // net::NetworkDelegate implementation.
  bool OnCanAccessFile(const net::URLRequest& request,
                       const base::FilePath& original_path,
                       const base::FilePath& absolute_path) const override;

  DISALLOW_COPY_AND_ASSIGN(ShellNetworkDelegate);
};

}  // namespace web

#endif  // IOS_WEB_SHELL_SHELL_NETWORK_DELEGATE_H_
