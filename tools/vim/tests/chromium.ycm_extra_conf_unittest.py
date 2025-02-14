#!/usr/bin/env python

# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for chromium.ycm_extra_conf.

These tests should be getting picked up by the PRESUBMIT.py in /tools/vim.
Currently the tests only run on Linux and require 'ninja' to be available on
PATH. Due to these requirements, the tests should only be run on upload.
"""

import imp
import os
import shutil
import stat
import string
import subprocess
import sys
import tempfile
import unittest

def CreateFile(path,
               copy_from = None,
               format_with = None,
               make_executable = False):
  """Creates a file.

  If a file already exists at |path|, it will be overwritten.

  Args:
    path: (String) Absolute path for file to be created.
    copy_from: (String or None) Absolute path to source file. If valid, the
               contents of this file will be written to |path|.
    format_with: (Dictionary or None) Only valid if |copy_from| is also valid.
               The contents of the file at |copy_from| will be passed through
               string.Formatter.vformat() with this parameter as the dictionary.
    make_executable: (Boolean) If true, |file| will be made executable.
  """
  if not os.path.isabs(path):
    raise Exception(
        'Argument |path| needs to be an absolute path. Got: "{}"'.format(path))
  with open(path, 'w') as f:
    if copy_from:
      with open(copy_from, 'r') as source:
        contents = source.read()
        if format_with:
          formatter = string.Formatter()
          contents = formatter.vformat(contents, None, format_with)
        f.write(contents)
  if make_executable:
    statinfo = os.stat(path)
    os.chmod(path, statinfo.st_mode | stat.S_IXUSR)

@unittest.skipIf(sys.platform.startswith('linux'),
                 'Tests are only valid on Linux.')
class Chromium_ycmExtraConfTest_NotOnLinux(unittest.TestCase):
  def testAlwaysFailsIfNotRunningOnLinux(self):
    self.fail('Changes to chromium.ycm_extra_conf.py currently need to be ' \
              'uploaded from Linux since the tests only run on Linux.')

@unittest.skipUnless(sys.platform.startswith('linux'),
                     'Tests are only valid on Linux.')
class Chromium_ycmExtraConfTest(unittest.TestCase):

  def SetUpFakeChromeTreeBelowPath(self):
    """Create fake Chromium source tree under self.test_root.

    The fake source tree has the following contents:

    <self.test_root>
      |  .gclient
      |
      +-- src
      |   |  DEPS
      |   |  three.cc
      |   |
      |   +-- .git
      |
      +-- out
          |
          +-- Debug
                build.ninja
    """
    self.chrome_root = os.path.abspath(os.path.normpath(
        os.path.join(self.test_root, 'src')))
    self.out_dir = os.path.join(self.chrome_root, 'out', 'Debug')

    os.makedirs(self.chrome_root)
    os.makedirs(os.path.join(self.chrome_root, '.git'))
    os.makedirs(self.out_dir)

    CreateFile(os.path.join(self.test_root, '.gclient'))
    CreateFile(os.path.join(self.chrome_root, 'DEPS'))
    CreateFile(os.path.join(self.chrome_root, 'three.cc'))

    # Fake ninja build file. Applications of 'cxx' rule are tagged by which
    # source file was used as input so that the test can verify that the correct
    # build dependency was used.
    CreateFile(os.path.join(self.out_dir, 'build.ninja'),
               copy_from=os.path.join(self.test_data_path,
                                      'fake_build_ninja.txt'))

  def NormalizeString(self, string):
    return string.replace(self.out_dir, '[OUT]').\
        replace(self.chrome_root, '[SRC]')

  def NormalizeStringsInList(self, list_of_strings):
    return [self.NormalizeString(s) for s in list_of_strings]

  def setUp(self):
    self.actual_chrome_root = os.path.normpath(
        os.path.join(os.path.dirname(os.path.abspath(__file__)), '../../..'))
    sys.path.append(os.path.join(self.actual_chrome_root, 'tools', 'vim'))
    self.test_data_path = os.path.join(self.actual_chrome_root, 'tools', 'vim',
                                       'tests', 'data')
    self.ycm_extra_conf = imp.load_source('ycm_extra_conf',
                                          'chromium.ycm_extra_conf.py')
    self.test_root = tempfile.mkdtemp()
    self.SetUpFakeChromeTreeBelowPath()

  def tearDown(self):
    if self.test_root:
      shutil.rmtree(self.test_root)

  def testNinjaIsAvailable(self):
    p = subprocess.Popen(['ninja', '--version'], stdout=subprocess.PIPE)
    _, _ = p.communicate()
    self.assertFalse(p.returncode)

  def testFindChromeSrc(self):
    chrome_source = self.ycm_extra_conf.FindChromeSrcFromFilename(
        os.path.join(self.chrome_root, 'chrome', 'one.cpp'))
    self.assertEquals(chrome_source, self.chrome_root)

    chrome_source = self.ycm_extra_conf.FindChromeSrcFromFilename(
        os.path.join(self.chrome_root, 'one.cpp'))
    self.assertEquals(chrome_source, self.chrome_root)

  def testCommandLineForKnownCppFile(self):
    command_line = self.ycm_extra_conf.GetClangCommandLineFromNinjaForSource(
        self.out_dir, os.path.join(self.chrome_root, 'one.cpp'))
    self.assertEquals(
        command_line, ('../../fake-clang++ -Ia -isysroot /mac.sdk -Itag-one '
                       '../../one.cpp -o obj/one.o'))

  def testCommandLineForUnknownCppFile(self):
    command_line = self.ycm_extra_conf.GetClangCommandLineFromNinjaForSource(
        self.out_dir, os.path.join(self.chrome_root, 'unknown.cpp'))
    self.assertEquals(command_line, None)

  def testGetClangOptionsForKnownCppFile(self):
    clang_options = \
        self.ycm_extra_conf.GetClangOptionsFromNinjaForFilename(
            self.chrome_root, os.path.join(self.chrome_root, 'one.cpp'))
    self.assertEquals(self.NormalizeStringsInList(clang_options), [
        '-I[SRC]',
        '-Wno-unknown-warning-option',
        '-I[OUT]/a',
        '-isysroot',
        '/mac.sdk',
        '-I[OUT]/tag-one'
        ])

  def testOutDirNames(self):
    out_root = os.path.join(self.chrome_root, 'out_with_underscore')
    out_dir = os.path.join(out_root, 'Debug')
    shutil.move(os.path.join(self.chrome_root, 'out'),
        out_root)

    clang_options = \
        self.ycm_extra_conf.GetClangOptionsFromNinjaForFilename(
            self.chrome_root, os.path.join(self.chrome_root, 'one.cpp'))
    self.assertIn('-I%s/a' % out_dir, clang_options)
    self.assertIn('-I%s/tag-one' % out_dir, clang_options)

  def testGetFlagsForFileForKnownCppFile(self):
    result = self.ycm_extra_conf.FlagsForFile(
        os.path.join(self.chrome_root, 'one.cpp'))
    self.assertTrue(result)
    self.assertTrue('do_cache' in result)
    self.assertTrue(result['do_cache'])
    self.assertTrue('flags' in result)
    self.assertEquals(self.NormalizeStringsInList(result['flags']), [
        '-DUSE_CLANG_COMPLETER',
        '-std=c++11',
        '-x', 'c++',
        '-I[SRC]',
        '-Wno-unknown-warning-option',
        '-I[OUT]/a',
        '-isysroot',
        '/mac.sdk',
        '-I[OUT]/tag-one'
        ])

  def testGetFlagsForFileForUnknownCppFile(self):
    result = self.ycm_extra_conf.FlagsForFile(
        os.path.join(self.chrome_root, 'nonexistent.cpp'))
    self.assertTrue(result)
    self.assertTrue('do_cache' in result)
    self.assertTrue(result['do_cache'])
    self.assertTrue('flags' in result)
    self.assertEquals(self.NormalizeStringsInList(result['flags']), [
        '-DUSE_CLANG_COMPLETER',
        '-std=c++11',
        '-x', 'c++',
        '-I[SRC]',
        '-Wno-unknown-warning-option',
        '-I[OUT]/a',
        '-isysroot',
        '/mac.sdk',
        '-I[OUT]/tag-default'
        ])

  def testGetFlagsForFileForUnknownHeaderFile(self):
    result = self.ycm_extra_conf.FlagsForFile(
        os.path.join(self.chrome_root, 'nonexistent.h'))
    self.assertTrue(result)
    self.assertTrue('do_cache' in result)
    self.assertTrue(result['do_cache'])
    self.assertTrue('flags' in result)
    self.assertEquals(self.NormalizeStringsInList(result['flags']), [
        '-DUSE_CLANG_COMPLETER',
        '-std=c++11',
        '-x', 'c++',
        '-I[SRC]',
        '-Wno-unknown-warning-option',
        '-I[OUT]/a',
        '-isysroot',
        '/mac.sdk',
        '-I[OUT]/tag-default'
        ])

  def testGetFlagsForFileForKnownHeaderFileWithAssociatedCppFile(self):
    result = self.ycm_extra_conf.FlagsForFile(
        os.path.join(self.chrome_root, 'three.h'))
    self.assertTrue(result)
    self.assertTrue('do_cache' in result)
    self.assertTrue(result['do_cache'])
    self.assertTrue('flags' in result)
    self.assertEquals(self.NormalizeStringsInList(result['flags']), [
        '-DUSE_CLANG_COMPLETER',
        '-std=c++11',
        '-x', 'c++',
        '-I[SRC]',
        '-Wno-unknown-warning-option',
        '-I[OUT]/a',
        '-isysroot',
        '/mac.sdk',
        '-I[OUT]/tag-three'
        ])

  def testSourceFileWithNonClangOutputs(self):
    # Verify assumption that four.cc has non-compiler-output listed as the first
    # output.
    p = subprocess.Popen(['ninja', '-C', self.out_dir, '-t',
                          'query', '../../four.cc'],
                         stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    stdout, _ = p.communicate()
    self.assertFalse(p.returncode)
    self.assertEquals(stdout,
                      '../../four.cc:\n'
                      '  outputs:\n'
                      '    obj/linker-output.o\n'
                      '    obj/four.o\n')

    result = self.ycm_extra_conf.FlagsForFile(
        os.path.join(self.chrome_root, 'four.cc'))
    self.assertTrue(result)
    self.assertTrue('do_cache' in result)
    self.assertTrue(result['do_cache'])
    self.assertTrue('flags' in result)
    self.assertEquals(self.NormalizeStringsInList(result['flags']), [
        '-DUSE_CLANG_COMPLETER',
        '-std=c++11',
        '-x', 'c++',
        '-I[SRC]',
        '-Wno-unknown-warning-option',
        '-I[OUT]/a',
        '-isysroot',
        '/mac.sdk',
        '-I[OUT]/tag-four'
        ])

  def testSourceFileWithOnlyNonClangOutputs(self):
    result = self.ycm_extra_conf.FlagsForFile(
        os.path.join(self.chrome_root, 'five.cc'))
    self.assertTrue(result)
    self.assertTrue('do_cache' in result)
    self.assertTrue(result['do_cache'])
    self.assertTrue('flags' in result)
    self.assertEquals(self.NormalizeStringsInList(result['flags']), [
        '-DUSE_CLANG_COMPLETER',
        '-std=c++11',
        '-x', 'c++',
        '-I[SRC]',
        '-Wno-unknown-warning-option',
        '-I[OUT]/a',
        '-isysroot',
        '/mac.sdk',
        '-I[OUT]/tag-default'
        ])

  def testGetFlagsForSysrootAbsPath(self):
    result = self.ycm_extra_conf.FlagsForFile(
        os.path.join(self.chrome_root, 'six.cc'))
    self.assertTrue(result)
    self.assertTrue('do_cache' in result)
    self.assertTrue(result['do_cache'])
    self.assertTrue('flags' in result)
    self.assertEquals(self.NormalizeStringsInList(result['flags']), [
        '-DUSE_CLANG_COMPLETER',
        '-std=c++11',
        '-x', 'c++',
        '-I[SRC]',
        '-Wno-unknown-warning-option',
        '-I[OUT]/a',
        '--sysroot=/usr/lib/sysroot-image',
        ])

  def testGetFlagsForSysrootRelPath(self):
    result = self.ycm_extra_conf.FlagsForFile(
        os.path.join(self.chrome_root, 'seven.cc'))
    self.assertTrue(result)
    self.assertTrue('do_cache' in result)
    self.assertTrue(result['do_cache'])
    self.assertTrue('flags' in result)
    self.assertEquals(self.NormalizeStringsInList(result['flags']), [
        '-DUSE_CLANG_COMPLETER',
        '-std=c++11',
        '-x', 'c++',
        '-I[SRC]',
        '-Wno-unknown-warning-option',
        '-I[OUT]/a',
        '--sysroot=[SRC]/build/sysroot-image',
        ])

if __name__ == '__main__':
  unittest.main()
