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

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <unistd.h>
#include <iostream>
#include <stdexcept>

#include <cmath>
#include <cstdlib>

#include <algorithm>

/* local includes */
#include "MPDRenderer.h"
#include "MyLogger.h"
#include "MyPLTController.h"

/**
 *
 */
MPDRenderer::MPDRenderer(int proxyLog) : current_controller_(nullptr), item_(nullptr) {
  ML_ENTRY_EXIT();

  mpd_.on_metadata([](const MpdController::Metadata& m) {
    ML_ENTRY_EXIT();
    ML_LOG_TRACE("Artist: %s\nTitle : %s\nAlbum : %s\n", m.artist.c_str(), m.title.c_str(), m.album.c_str());
  });

  mpd_.on_position([this](const MpdController::PositionSnapshot& p) {
    ML_ENTRY_EXIT();
    auto secs = [](uint32_t ms) { return ms / 1000; };

    ML_LOG_TRACE("[POS] %s  %lds / %lds | gapless=%s prefetched=%s\n",
                 (p.state == MPD_STATE_PLAY    ? "PLAY "
                  : p.state == MPD_STATE_PAUSE ? "PAUSE"
                  : p.state == MPD_STATE_STOP  ? "STOP "
                                               : "UNK  "),
                 secs(p.elapsed_ms),
                 secs(p.total_ms),
                 (p.gapless ? "yes" : "no"),
                 (p.next_song_prefetched ? "yes" : "no"));

    if (current_controller_) {
#if __cplusplus >= 201703L
      current_controller_->send(UpdatePlayTimeMessage{secs(p.elapsed_ms), secs(p.total_ms)});
#else
      current_controller_->send(std::make_shared<UpdatePlayTimeMessage>(secs(p.elapsed_ms), secs(p.total_ms)));
#endif
    }
  });

  mpd_.on_track_ended([this]() {
    ML_ENTRY_EXIT();

    if (current_controller_) {
#if __cplusplus >= 201703L
      current_controller_->send(PlayNextMessage{});
#else
      current_controller_->send(std::make_shared<PlayNextMessage>());
#endif
    }
  });
}

/**
 *
 */
MPDRenderer::~MPDRenderer() {
  ML_ENTRY_EXIT();

  std::lock_guard<std::mutex> lock(mutex_);

  mpd_.clear();

  /* this is equale to item_.reset(); */
  item_ = nullptr;
}

/**
 *
 */
int MPDRenderer::play(IMyPLTController* controller, std::shared_ptr<MediaItem> item) {
  ML_ENTRY_EXIT();

  int ret = RendererStatus::Error;

  std::lock_guard<std::mutex> lock(mutex_);

  if ((current_controller_ == NULL) || (current_controller_ == controller)) {
    current_controller_ = controller;

    mpd_.clear();

    synchronized_status_.state = RendererState::Stopped;

    item_ = nullptr;

    if (item) {
      item_ = item;
      ML_LOG_DEBUG("    call playOpen(%s)\n", item->uri.c_str());

      /* open URI */
      mpd_.add(item->uri.c_str());

      /* start playback */
      mpd_.play();

      synchronized_status_.state = RendererState::Playing;

      ret = RendererStatus::NoError;
    }
  } else {
    /* not accept play */
  }

out:
  if (ret != RendererStatus::NoError) {
    ML_LOG_ERROR("-> MPDRenderer::play failed with [%s]\n", RendererStatus::toString(ret));

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
int MPDRenderer::pause(IMyPLTController* controller) {
  ML_ENTRY_EXIT();

  std::lock_guard<std::mutex> lock(mutex_);

  if (current_controller_ == controller) {
    if (synchronized_status_.state == RendererState::Playing) {
      mpd_.pause(true);
      synchronized_status_.state = RendererState::Paused;
    }
  }

  return RendererStatus::NoError;
}

/**
 *
 */
int MPDRenderer::unpause(IMyPLTController* controller) {
  ML_ENTRY_EXIT();

  std::lock_guard<std::mutex> lock(mutex_);

  if (current_controller_ == controller) {
    if (synchronized_status_.state == RendererState::Paused) {
      mpd_.pause(false);
      synchronized_status_.state = RendererState::Playing;
    }
  }

  return RendererStatus::NoError;
}

/**
 *
 */
int MPDRenderer::stop(IMyPLTController* controller) {
  ML_ENTRY_EXIT();

  std::lock_guard<std::mutex> lock(mutex_);

  if (current_controller_ == controller) {
    mpd_.stop();
    synchronized_status_.state = RendererState::Stopped;

    current_controller_ = NULL;
  }

  return RendererStatus::NoError;
}

/**
 *
 */
int MPDRenderer::seek(IMyPLTController* controller, int32_t rel, int32_t value) {
  ML_ENTRY_EXIT();

  std::lock_guard<std::mutex> lock(mutex_);

  if (current_controller_ == controller) {
    if ((synchronized_status_.state == RendererState::Playing) || (synchronized_status_.state == RendererState::Paused)) {
      try {
        if (rel) {
          auto fut = mpd_.seek(value, true);
          fut.get(); // wait for completion
        } else {
          auto fut = mpd_.seek(value, false);
          fut.get(); // wait for completion
        }
      } catch (std::exception& e) {
        ML_LOG_ERROR("seek failed with [%s]\n", e.what());
      }
    }
  }

  return RendererStatus::NoError;
}

/**
 *
 */
int MPDRenderer::setVolume(IMyPLTController* controller, uint32_t value) {
  ML_ENTRY_EXIT();

  std::lock_guard<std::mutex> lock(mutex_);

  if ((current_controller_ == NULL) || (current_controller_ == controller)) {
    try {
      auto fut = mpd_.set_volume(value);
      fut.get(); // wait for completion

      synchronized_status_.volume = value;
      updateControllers();
    } catch (std::exception& e) {
      ML_LOG_ERROR("set_volume failed with [%s]\n", e.what());
    }
  }

  return RendererStatus::NoError;
}

/**
 *
 */
int MPDRenderer::setMute(IMyPLTController* controller, int32_t value) {
  ML_ENTRY_EXIT();

  std::lock_guard<std::mutex> lock(mutex_);

  if ((current_controller_ == NULL) || (current_controller_ == controller)) {
    try {
      if (value) {
        auto fut = mpd_.set_volume(0);
        fut.get(); // wait for completion
      } else {
        auto fut = mpd_.set_volume(synchronized_status_.volume);
        fut.get(); // wait for completion
      }
      synchronized_status_.mute = value;
      updateControllers();
    } catch (std::exception& e) {
      ML_LOG_ERROR("set_volume failed with [%s]\n", e.what());
    }
  }

  return RendererStatus::NoError;
}

/**
 *
 */
int MPDRenderer::setRepeat(IMyPLTController* controller, int32_t value) {
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
int MPDRenderer::setShuffle(IMyPLTController* controller, int32_t value) {
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
void MPDRenderer::updateIdleControllers() {
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
void MPDRenderer::updateControllers() {
  ML_ENTRY_EXIT();

  for (auto controller : controllers_) {
    controller->RendererChanges(&synchronized_status_);
  }
}
