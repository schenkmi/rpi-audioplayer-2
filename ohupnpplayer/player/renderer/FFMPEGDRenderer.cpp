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
#include <algorithm>

/* local includes */
#include "FFMPEGDRenderer.h"
#include "MyLogger.h"
#include "MyPLTController.h"

using namespace std;
using namespace albisffmpeg;

/**
 * SNK:
 * Functions need to be mutex protected here
 */

/**
 *
 */
FFMPEGDRenderer::FFMPEGDRenderer(int proxyLog) : FFMPEGProxyController(proxyLog), current_controller_(nullptr), item_(nullptr) {
  ML_ENTRY_EXIT();

  int ret = 0;
  if ((ret = create(FFMPEGProxyControllerMode::SenderListener)) != FFMPEGProxyStatus::NoError) {
    ML_LOG_ERROR("create ffmpeg proxy controller failed with [%s]\n", FFMPEGProxyStatus::toString(ret));
  }
}

/**
 *
 */
FFMPEGDRenderer::~FFMPEGDRenderer() {
  ML_ENTRY_EXIT();

  std::lock_guard<std::mutex> lock(mutex_);

  /* this is equale to item_.reset(); */
  item_ = nullptr;

  /* for safety reasons */
  deactivate();
}

/**
 *
 */
int FFMPEGDRenderer::play(IMyPLTController* controller, std::shared_ptr<MediaItem> item) {
  ML_ENTRY_EXIT();

  int ret = RendererStatus::Error;

  std::lock_guard<std::mutex> lock(mutex_);

  if ((current_controller_ == NULL) || (current_controller_ == controller)) {
    current_controller_ = controller;

    playClose();
    synchronized_status_.state = RendererState::Stopped;

    item_ = nullptr;

    if (item) {
      item_ = item;
      ML_LOG_DEBUG("    call playOpen(%s)\n", item->uri.c_str());

      /* open URI */
      if ((ret = playOpen(item->uri.c_str())) == FFMPEGProxyStatus::ErrorNotActivated) {
        /* not activated, activate and try again */
        activate();

        /* set default volume and mute */
        audioSetVolume(synchronized_status_.volume, synchronized_status_.volume);

        audioSetMute(synchronized_status_.mute);

        ret = playOpen(item->uri.c_str());
      }

      if (ret != FFMPEGProxyStatus::NoError) {
        ret = RendererStatus::Error;
        goto out;
      }

      /* start playback */
      if ((ret = playStart()) != FFMPEGProxyStatus::NoError) {
        ML_LOG_ERROR("-> FFMPEGDRenderer::play playStart failed with [%s]\n", FFMPEGProxyStatus::toString(ret));
        playClose();
        ret = RendererStatus::Error;
        goto out;
      }

      synchronized_status_.state = RendererState::Playing;

      ret = RendererStatus::NoError;
    }
  } else {
    /* not accept play */
  }

out:
  if (ret != RendererStatus::NoError) {
    ML_LOG_ERROR("-> FFMPEGDRenderer::play failed with [%s]\n", RendererStatus::toString(ret));

    /* try to iterate to next track */
    if (current_controller_) {
#if __cplusplus >= 201703L
      current_controller_->send(PlayNextMessage{});
#else
      current_controller_->send(std::make_shared<PlayNextMessage>());
#endif
    }
  }

  return ret;
}

/**
 *
 */
int FFMPEGDRenderer::pause(IMyPLTController* controller) {
  ML_ENTRY_EXIT();

  std::lock_guard<std::mutex> lock(mutex_);

  if (current_controller_ == controller) {
    if (synchronized_status_.state == RendererState::Playing) {
      playPause();

      synchronized_status_.state = RendererState::Paused;
    }
  }

  return RendererStatus::NoError;
}

/**
 *
 */
int FFMPEGDRenderer::unpause(IMyPLTController* controller) {
  ML_ENTRY_EXIT();

  std::lock_guard<std::mutex> lock(mutex_);

  if (current_controller_ == controller) {
    if (synchronized_status_.state == RendererState::Paused) {
      playResume();

      synchronized_status_.state = RendererState::Playing;
    }
  }

  return RendererStatus::NoError;
}

/**
 *
 */
int FFMPEGDRenderer::stop(IMyPLTController* controller) {
  ML_ENTRY_EXIT();

  std::lock_guard<std::mutex> lock(mutex_);

  ML_LOG_DEBUG("-> FFMPEGDRenderer::stop\n");

  if (current_controller_ == controller) {
    playClose();
    synchronized_status_.state = RendererState::Stopped;

    /* deactivates the proxy */
    deactivate();
    current_controller_ = NULL;
  }

  ML_LOG_DEBUG("-> FFMPEGDRenderer::stop done\n");

  return RendererStatus::NoError;
}

/**
 *
 */
int FFMPEGDRenderer::seek(IMyPLTController* controller, int32_t rel, int32_t value) {
  ML_ENTRY_EXIT();

  std::lock_guard<std::mutex> lock(mutex_);

  if (current_controller_ == controller) {
    if ((synchronized_status_.state == RendererState::Playing) || (synchronized_status_.state == RendererState::Paused)) {
      if (rel) {
        playSeek(FFMPEGProxySeekTag::TimeRelative, value);
      } else {
        playSeek(FFMPEGProxySeekTag::Time, value);
      }
    }
  }

  /* update play position ? */

  return RendererStatus::NoError;
}

/**
 *
 */
int FFMPEGDRenderer::setVolume(IMyPLTController* controller, uint32_t value) {
  ML_ENTRY_EXIT();

  std::lock_guard<std::mutex> lock(mutex_);

  if ((current_controller_ == NULL) || (current_controller_ == controller)) {
    synchronized_status_.volume = value;

    audioSetVolume(synchronized_status_.volume, synchronized_status_.volume);

    updateControllers();
  }

  return RendererStatus::NoError;
}

/**
 *
 */
int FFMPEGDRenderer::setMute(IMyPLTController* controller, int32_t value) {
  ML_ENTRY_EXIT();

  std::lock_guard<std::mutex> lock(mutex_);

  if ((current_controller_ == NULL) || (current_controller_ == controller)) {
    synchronized_status_.mute = value;

    audioSetMute(synchronized_status_.mute);

    updateControllers();
  }

  return RendererStatus::NoError;
}

/**
 *
 */
int FFMPEGDRenderer::setRepeat(IMyPLTController* controller, int32_t value) {
  ML_ENTRY_EXIT();

  std::lock_guard<std::mutex> lock(mutex_);

  if ((current_controller_ == NULL) || (current_controller_ == controller)) {
    synchronized_status_.repeat = value;
    updateControllers();
  }

  return RendererStatus::NoError;
}

/**
 *
 */
int FFMPEGDRenderer::setShuffle(IMyPLTController* controller, int32_t value) {
  ML_ENTRY_EXIT();

  std::lock_guard<std::mutex> lock(mutex_);

  if ((current_controller_ == NULL) || (current_controller_ == controller)) {
    synchronized_status_.shuffle = value;
    updateControllers();
  }

  return RendererStatus::NoError;
}

/**
 *
 */
void FFMPEGDRenderer::onDecodingInfoUpdate(FFMPEGProxyDecodingInfoUpdateParams* params) {
  ML_ENTRY_EXIT();

  if (params) {
    m_DecodingInfoUpdateParams = *params;

    switch (params->state) {
      case FFMPEGProxyState::Closed:
        // printf("-> state FFMPEGProxyState::Closed\n");
        break;
      case FFMPEGProxyState::Opened:
        // printf("-> state FFMPEGProxyState::Opened\n");
        m_DecodingInfoUpdateParams = *params;
        break;
      case FFMPEGProxyState::Running:
        // printf("-> state FFMPEGProxyState::Running\n");
        m_DecodingInfoUpdateParams = *params;

        if (current_controller_) {
#if __cplusplus >= 201703L
          current_controller_->send(UpdatePlayTimeMessage{params->time, params->duration});
#else
          current_controller_->send(std::make_shared<UpdatePlayTimeMessage>(params->time, params->duration));
#endif
        }
        break;
      default:
        break;
    }
  }
}

/**
 *
 */
void FFMPEGDRenderer::onError() {
  ML_ENTRY_EXIT();

  if (current_controller_) {
#if __cplusplus >= 201703L
    current_controller_->send(PlayNextMessage{});
#else
    current_controller_->send(std::make_shared<PlayNextMessage>());
#endif
  }
}

/**
 *
 */
void FFMPEGDRenderer::onStartOfStream() {
  ML_ENTRY_EXIT();
}

/**
 *
 */
void FFMPEGDRenderer::onEndOfStream() {
  ML_ENTRY_EXIT();

  if (current_controller_) {
#if __cplusplus >= 201703L
    current_controller_->send(PlayNextMessage{});
#else
    current_controller_->send(std::make_shared<PlayNextMessage>());
#endif
  }
}

/**
 *
 */
void FFMPEGDRenderer::onUnifiedSubtitleUpdate(FFMPEGProxyUnifiedSubtitleParams* params) {
  ML_ENTRY_EXIT();

  params = params;
}

/**
 *
 */
void FFMPEGDRenderer::onStreamInfo(FFMPEGProxyStreamInfoParams* params) {
  ML_ENTRY_EXIT();

  m_streamInfoParams = *params;
}

/**
 *
 */
void FFMPEGDRenderer::updateIdleControllers() {
  ML_ENTRY_EXIT();

  for (auto controller : controllers_) {
    if (current_controller_ != controller) {
      controller->RendererChanges(&synchronized_status_);
    }
  }
}

/**
 *
 */
void FFMPEGDRenderer::updateControllers() {
  ML_ENTRY_EXIT();

  for (auto controller : controllers_) {
    controller->RendererChanges(&synchronized_status_);
  }
}
