# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from benchmarks import rendering
from telemetry import benchmark


@benchmark.Owner(emails=['sadrul@chromium.org', 'kylechar@chromium.org'])
class RenderingDesktopOOPD(rendering.RenderingDesktop):
  tag = 'oopd'

  @classmethod
  def Name(cls):
    return 'rendering.oopd.desktop'

  def SetExtraBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--enable-features=VizDisplayCompositor')
