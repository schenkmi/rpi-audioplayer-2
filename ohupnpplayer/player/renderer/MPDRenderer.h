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

#include <atomic>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
/* local includes */
#include <MpdController.h>
#include <Renderer.h>

class IMyPLTController;

/**
 * Implemeting MPD renderer
 */
class MPDRenderer : public IRenderer {
 public:
  MPDRenderer(int proxyLog);
  ~MPDRenderer();

  virtual int play(IMyPLTController* controller, std::shared_ptr<MediaItem> item);
  virtual int pause(IMyPLTController* controller);
  virtual int unpause(IMyPLTController* controller);
  virtual int stop(IMyPLTController* controller);
  virtual int seek(IMyPLTController* controller, int32_t rel, int32_t value);
  virtual int setVolume(IMyPLTController* controller, uint32_t value);
  virtual int setMute(IMyPLTController* controller, int32_t value);
  virtual int setRepeat(IMyPLTController* controller, int32_t value);
  virtual int setShuffle(IMyPLTController* controller, int32_t value);

  virtual void updateIdleControllers();
  virtual void updateControllers();

 private:
  IMyPLTController* current_controller_;
  std::shared_ptr<MediaItem> item_;
  std::mutex mutex_;
  MpdController mpd_;
};
