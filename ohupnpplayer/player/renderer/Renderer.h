/**
 * OHUPnPPlayer (OpenHome/UPnP/DLNA Player Daemon)
 *
 * Copyright (c) 2025-2026, Schenk Engineering
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
#pragma once

#include <MediaItem.h>

/**
 * Generic anon enum for RenderState
 */
class RendererState {
 public:
  enum {
    Stopped = 0,      /* stopped state              */
    First = Stopped,  /* start marker of anon enum  */
    Playing,          /* playing state              */
    Paused,           /* paused state               */
    Buffering,        /* buffering state            */
    Last = Buffering, /* end marker of anon enum    */
  };

  static bool validate(int value) {
    return ((value >= First) && (value <= Last));
  }

  static const char* toString(int value) {
    static const char* text[] = {
        "stopped",
        "playing",
        "paused",
        "buffering",
    };

    return validate(value) ? text[value] : "unknown";
  }
};

class RendererStatus {
 public:
  enum {
    NoError = 0,     /* no error                  */
    First = NoError, /* start marker of anon enum */
    Error,           /* generic error             */
    Last = Error,    /* end marker of anon enum   */
  };

  static bool validate(int value) {
    return ((value >= First) && (value <= Last));
  }

  static const char* toString(int value) {
    static const char* text[] = {
        "no error",
        "error",
    };

    return validate(value) ? text[value] : "unknown";
  }
};

class IMyPLTController;

using IMyPLTControllers = std::vector<IMyPLTController*>;
using IMyPLTControllersIt = IMyPLTControllers::iterator;
using IMyPLTControllersConstIt = IMyPLTControllers::const_iterator;

class SynchronizedStatus {
 public:
  SynchronizedStatus() : mute(0), volume(100), repeat(0), shuffle(0), state(RendererState::Stopped) {}

 public:
  int32_t mute;
  int32_t volume;
  int32_t repeat;
  int32_t shuffle;
  int32_t state;
};

/**
 * Renderer (player) interface class
 */
class IRenderer {
 public:
  IRenderer() {}
  virtual ~IRenderer() {}

  virtual int play(IMyPLTController* controller, std::shared_ptr<MediaItem> item) = 0;
  virtual int pause(IMyPLTController* controller) = 0;
  virtual int unpause(IMyPLTController* controller) = 0;
  virtual int stop(IMyPLTController* controller) = 0;
  virtual int seek(IMyPLTController* controller, int32_t rel, int32_t value) = 0;
  virtual int setVolume(IMyPLTController* controller, uint32_t value) = 0;
  virtual int setMute(IMyPLTController* controller, int32_t value) = 0;
  virtual int setRepeat(IMyPLTController* controller, int32_t value) = 0;
  virtual int setShuffle(IMyPLTController* controller, int32_t value) = 0;

  virtual int32_t getMute() {
    return synchronized_status_.mute;
  }
  virtual uint32_t getVolume() {
    return synchronized_status_.volume;
  }
  virtual uint32_t getRepeat() {
    return synchronized_status_.repeat;
  }
  virtual uint32_t getShuffle() {
    return synchronized_status_.shuffle;
  }
  virtual int getState() {
    return synchronized_status_.state;
  }

  virtual void updateIdleControllers() = 0;
  virtual void updateControllers() = 0;

  int registerNotifier(IMyPLTController* controller) {
    controllers_.push_back(controller);

    updateControllers();

    return 0;
  }

 protected:
  IMyPLTControllers controllers_;
  SynchronizedStatus synchronized_status_;
};
