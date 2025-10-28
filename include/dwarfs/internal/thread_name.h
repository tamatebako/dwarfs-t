/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Marcus Holland-Moritz (github@mhxnet.de)
 * \copyright  Copyright (c) Marcus Holland-Moritz
 *
 * This file is part of dwarfs.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

/**
 * \file thread_name.h
 * \brief Thread naming utilities (folly::setThreadName replacement)
 *
 * Provides platform-specific thread naming functionality.
 */

#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

namespace dwarfs::compat {

/**
 * \brief Set name for current thread
 *
 * Platform-specific implementation of thread naming.
 * On Linux/macOS uses pthread_setname_np.
 * On Windows uses SetThreadDescription (Windows 10 1607+).
 *
 * \param name Thread name (may be truncated on some platforms)
 * \return true on success, false on failure
 */
inline bool setThreadName(const std::string& name) {
#if defined(__linux__)
  // Linux: pthread_setname_np(pthread_self(), name)
  // Thread name limited to 16 bytes including null terminator
  return pthread_setname_np(pthread_self(), name.substr(0, 15).c_str()) == 0;

#elif defined(__APPLE__)
  // macOS: pthread_setname_np(name) - only sets current thread
  // Thread name limited to 64 bytes
  return pthread_setname_np(name.substr(0, 63).c_str()) == 0;

#elif defined(_WIN32)
  // Windows 10 1607+ has SetThreadDescription
  #if defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x0A00
  // Convert to wide string
  int size_needed = MultiByteToWideChar(CP_UTF8, 0, name.c_str(),
                                        static_cast<int>(name.size()),
                                        nullptr, 0);
  std::wstring wname(size_needed, 0);
  MultiByteToWideChar(CP_UTF8, 0, name.c_str(),
                      static_cast<int>(name.size()),
                      &wname[0], size_needed);

  HRESULT hr = SetThreadDescription(GetCurrentThread(), wname.c_str());
  return SUCCEEDED(hr);
  #else
  // Older Windows versions don't support thread naming via API
  (void)name;
  return false;
  #endif

#else
  // Unsupported platform
  (void)name;
  return false;
#endif
}

} // namespace dwarfs::compat