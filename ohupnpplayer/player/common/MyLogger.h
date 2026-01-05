/**
 * OHUPnPPlayer (OpenHome/UPnP/DLNA Player Daemon)
 *
 * Copyright (c) 2015-2016, Schenk Engineering
 * All Rights Reserved
 *
 * Author: Michael Schenk
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * OEMs, ISVs, VARs and other distributors that combine and distribute
 * commercially licensed software with Schenk Engineering software
 * and do not wish to distribute the source code for the commercially
 * licensed software under version 2, or (at your option) any later
 * version, of the GNU General Public License (the "GPL") must enter
 * into a commercial license agreement with Schenk Engineering.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file LICENSE.txt. If not, write to
 * the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 * http://www.gnu.org/licenses/gpl-2.0.html
 */

#if 0

#pragma once

#include <stdio.h>

extern int _mylogger_log_level_;

#define ML_LOG_MASK 0x0F

#define ML_ENTRY_EXIT() MLEntryStackObject obj##__LINE__(__PRETTY_FUNCTION__, _mylogger_log_level_);

struct MLEntryStackObject {
  MLEntryStackObject(const char* f, int log_level) : f_(f), log_level_(log_level) {
    if (log_level_ > ML_LOG_MASK) {
      printf("<- @func [%s]\n", f_);
    }
  }
  ~MLEntryStackObject() {
    if (log_level_ > ML_LOG_MASK) {
      printf("-> @func [%s]\n", f_);
    }
  }
  const char* f_;
  int log_level_;
};

#define ML_LOG_ERROR(format, arg...) \
  { fprintf(stderr, "*** " format, ##arg); }
#define ML_LOG_INFO(format, arg...)                 \
  {                                                 \
    if ((_mylogger_log_level_ & ML_LOG_MASK) > 0) { \
      fprintf(stderr, format, ##arg);               \
    }                                               \
  }
#define ML_LOG_DEBUG(format, arg...)                \
  {                                                 \
    if ((_mylogger_log_level_ & ML_LOG_MASK) > 1) { \
      fprintf(stderr, format, ##arg);               \
    }                                               \
  }
#define ML_LOG_TRACE(format, arg...)                \
  {                                                 \
    if ((_mylogger_log_level_ & ML_LOG_MASK) > 2) { \
      fprintf(stderr, format, ##arg);               \
    }                                               \
  }
#else
#pragma once

#include <cstdarg>
#include <cstdio>

extern int _mylogger_log_level_;

constexpr int ML_LOG_MASK = 0x0F;

enum class MLLogLevel : int {
  NONE = 0,
  ERROR = 1,
  INFO = 2,
  DEBUG = 3,
  TRACE = 4
};

// RAII helper for function entry/exit
struct MLEntryStackObject {
  MLEntryStackObject(const char* func, int log_level) : func_(func), log_level_(log_level) {
    if (log_level_ > ML_LOG_MASK) {
      std::printf("<- @func [%s]\n", func_);
    }
  }

  ~MLEntryStackObject() {
    if (log_level_ > ML_LOG_MASK) {
      std::printf("-> @func [%s]\n", func_);
    }
  }

 private:
  const char* func_;
  int log_level_;
};

#define ML_ENTRY_EXIT() MLEntryStackObject obj_##__LINE__(__PRETTY_FUNCTION__, _mylogger_log_level_);

// Logging functions
inline void ML_LOG_ERROR(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  std::fprintf(stderr, "*** ");
  std::vfprintf(stderr, fmt, args);
  va_end(args);
}

inline void ML_LOG_INFO(const char* fmt, ...) {
  if ((_mylogger_log_level_ & ML_LOG_MASK) > 0) {
    va_list args;
    va_start(args, fmt);
    std::vfprintf(stderr, fmt, args);
    va_end(args);
  }
}

inline void ML_LOG_DEBUG(const char* fmt, ...) {
  if ((_mylogger_log_level_ & ML_LOG_MASK) > 1) {
    va_list args;
    va_start(args, fmt);
    std::vfprintf(stderr, fmt, args);
    va_end(args);
  }
}

inline void ML_LOG_TRACE(const char* fmt, ...) {
  if ((_mylogger_log_level_ & ML_LOG_MASK) > 2) {
    va_list args;
    va_start(args, fmt);
    std::vfprintf(stderr, fmt, args);
    va_end(args);
  }
}

#endif
