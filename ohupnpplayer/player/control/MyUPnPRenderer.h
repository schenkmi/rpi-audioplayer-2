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
#pragma once

/* Platinum/Neptune UPnP SDK includes */
#include <PltMediaItem.h>
#include <PltMediaRenderer.h>
/* local includes */
#include <MyPLTController.h>

/**
 *
 */
class MyUPnPRenderer : public IMyPLTController, public PLT_MediaRenderer {
 public:
  MyUPnPRenderer(std::shared_ptr<IRenderer> renderer, const char* friendly_name, bool show_ip = false, const char* uuid = NULL, unsigned int port = 0);

  virtual ~MyUPnPRenderer();

  /* render interface */

  virtual const char* GetName() {
    return "MyUPnPRenderer";
  }

  /**
   * Called by the renderer (decoder) if synchronized
   * stuff has changed (mute, volume, shuffle, repeat).
   */
  virtual void RendererChanges(SynchronizedStatus* status);

 private:
  /**
   * inherent functions from PLT_MediaRenderer class
   */

  /* AVTransport methods */
  virtual NPT_Result OnNext(PLT_ActionReference& action);
  virtual NPT_Result OnPause(PLT_ActionReference& action);
  virtual NPT_Result OnPlay(PLT_ActionReference& action);
  virtual NPT_Result OnPrevious(PLT_ActionReference& action);
  virtual NPT_Result OnStop(PLT_ActionReference& action);
  virtual NPT_Result OnSeek(PLT_ActionReference& action);
  virtual NPT_Result OnSetAVTransportURI(PLT_ActionReference& action);

  /* RenderingControl methods */
  virtual NPT_Result OnSetVolume(PLT_ActionReference& action);
  virtual NPT_Result OnSetMute(PLT_ActionReference& action);
  virtual NPT_Result OnGetVolumeDBRange(PLT_ActionReference& action);

  /* helper functions */
  NPT_Result SetupServices();
  void UpdateState();
  void UpdateStateUnlocked();

  std::shared_ptr<MediaItem> media_item_;
};
