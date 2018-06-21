#!/usr/bin/env python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Clones an Android Emulator image.

Create an emulator image via Android Studio and then use this script to clone
it. Cloning an image enables you to run multiple emulators at the same time, and
thus run tests faster.
"""
import argparse
import os
import re
import shutil


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('--source-ini', required=True,
                      help='Path to ~/.android/avd/source.ini to clone')
  parser.add_argument('--dest-ini', required=True,
                      help='Path to ~/.android/avd/dest.ini to create')
  parser.add_argument('--display-name', required=True,
                      help='Name for emulator window & Android Studio list')

  args = parser.parse_args()

  avd_dir = os.path.dirname(args.source_ini)

  old_ini_path = args.source_ini
  old_avd_id = os.path.basename(old_ini_path)[:-4]
  old_avd_path = os.path.join(avd_dir, old_avd_id + '.avd')

  new_ini_path = args.dest_ini
  new_avd_id = os.path.basename(new_ini_path)[:-4]
  new_avd_path = os.path.join(avd_dir, new_avd_id + '.avd')

  if not re.match(r'\w+$', new_avd_id):
    parser.error('--dest-ini should be alphanumeric.')
  if not os.path.exists(old_ini_path):
    parser.error('Path does not exist: ' + old_ini_path)
  if not os.path.exists(old_avd_path):
    parser.error('Path does not exist: ' + old_avd_path)
  if os.path.exists(new_ini_path):
    parser.error('Destination path already exists: ' + new_ini_path)
  if os.path.exists(new_avd_path):
    parser.error('Destination path already exists: ' + new_avd_path)

  shutil.copy(old_ini_path, new_ini_path)
  with open(new_ini_path, 'r+') as f:
    data = f.read()
    data = data.replace(old_avd_id + '.avd', new_avd_id + '.avd')
    f.truncate(0)
    f.write(data)

  print 'Copying avd...'
  shutil.copytree(old_avd_path, new_avd_path,
                  ignore=shutil.ignore_patterns('*.lock'))

  with open(os.path.join(new_avd_path, 'config.ini'), 'r+') as f:
    data = f.read()
    # AvdId=Android_O_26_x86
    # avd.ini.displayname=Android O (26) x86
    data = re.sub(r'AvdId=.+', 'AvdId=' + new_avd_id, data)
    data = re.sub(r'avd.ini.displayname=.+',
                  'avd.ini.displayname=' + args.display_name,
                  data)
    f.truncate(0)
    f.write(data)

  print 'Done!'


if __name__ == '__main__':
  main()
