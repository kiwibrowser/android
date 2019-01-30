// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_BROWSER_IMAGE_REGISTRAR_H_
#define CHROME_BROWSER_UI_ASH_BROWSER_IMAGE_REGISTRAR_H_

#include "base/macros.h"

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "ui/gfx/image/image_skia.h"

// This class represents an image that's been registered with Ash. It is ref
// counted so that when multiple callsites want to use the same image, they can
// all hold a reference, and when they all release the reference the
// registration will destruct.
class ImageRegistration : public base::RefCounted<ImageRegistration> {
 public:
  ImageRegistration(const base::UnguessableToken& token,
                    const gfx::ImageSkia& image);

  const base::UnguessableToken& token() const { return token_; }

 protected:
  friend class base::RefCounted<ImageRegistration>;

  virtual ~ImageRegistration();

 private:
  const base::UnguessableToken token_;
  const gfx::ImageSkia image_;

  DISALLOW_COPY_AND_ASSIGN(ImageRegistration);
};

// A collection of functions to register and unregister images with Ash. This is
// used in Mash to minimize the duplication of images sent over mojo.
class BrowserImageRegistrar {
 public:
  // Must be called once when the browser process is exiting.
  static void Shutdown();

  // Gets or creates a registration for the given image. This registers the
  // image and token with Ash. The caller should hold onto the returned
  // object as long as the image is in use. When all refs to a given
  // registration are released, Ash will be informed and the associated token
  // will no longer be useful. This function also serves as a way to lazily
  // initialize the implementation object.
  static scoped_refptr<ImageRegistration> RegisterImage(
      const gfx::ImageSkia& image) WARN_UNUSED_RESULT;

  static std::vector<ImageRegistration*> GetActiveRegistrationsForTesting();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(BrowserImageRegistrar);
};

#endif  // CHROME_BROWSER_UI_ASH_BROWSER_IMAGE_REGISTRAR_H_
