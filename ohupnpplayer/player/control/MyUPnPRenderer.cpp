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
#include <assert.h>
#include <algorithm>

/* Platinum/Neptune UPnP SDK includes */
#include "PltDidl.h"
#include "PltService.h"
#include "PltUtilities.h"
/* local includes */
#include "MyLogger.h"
#include "MyUPnPRenderer.h"
#include "Renderer.h"

NPT_SET_LOCAL_LOGGER("platinum.upnp.myplaylist")

/**
 * Class constructor.
 */
MyUPnPRenderer::MyUPnPRenderer(std::shared_ptr<IRenderer> renderer, const char* friendly_name, bool show_ip, const char* uuid, unsigned int port)
    : IMyPLTController(renderer), PLT_MediaRenderer(friendly_name, show_ip, uuid, port, true) {
  ML_ENTRY_EXIT();

  renderer_->registerNotifier(this);

  registerHandler<UpdatePlayTimeMessage>([this](const UpdatePlayTimeMessage& msg) {
    ML_ENTRY_EXIT();

    ML_LOG_TRACE(" UpdatePlayTimeMessage time=%u, duration=%u\n", msg.time, msg.duration);

    std::lock_guard<std::mutex> lock(mutex_);

    elapsed_time_ = msg.time;
    track_time_ = msg.duration;

    PLT_Service* service = nullptr;
    if (NPT_SUCCEEDED(FindServiceByType("urn:schemas-upnp-org:service:AVTransport:1", service))) {
      EventingPauseGuard guard(service);

      NPT_String timeString = PLT_Didl::FormatTimeStamp(elapsed_time_);
      service->SetStateVariable("RelativeTimePosition", timeString);
      service->SetStateVariable("AbsoluteTimePosition", timeString);

      timeString = PLT_Didl::FormatTimeStamp(track_time_);
      service->SetStateVariable("CurrentTrackDuration", timeString);
      service->SetStateVariable("CurrentMediaDuration", timeString);
    }
  });

  registerHandler<PlayNextMessage>([this](const PlayNextMessage&) {
    ML_ENTRY_EXIT();

    /**
     * for iterate to the next track we need to fake the TransportState to STOPPED
     * but we do not like to call real playStop in that case.
     */
    NPT_String timeString;
    NPT_String currentTrackURI;
    NPT_String transportState;
    PLT_Service* service = nullptr;
    /* update A/V transport stuff */
    if (NPT_SUCCEEDED(FindServiceByType("urn:schemas-upnp-org:service:AVTransport:1", service))) {
      /* pause automatic eventing, we change multiple state vars */
      EventingPauseGuard guard(service);

/* WA for kinsky tracks not advanced */
#if 1
      /**
       * this will force to re-send the CurrentTrackURI together
       * with the changed TransportState. Kinsky rely on this :-(.
       */
      service->GetStateVariableValue("CurrentTrackURI", currentTrackURI);

      /* clear and set to activate changed flag */
      service->SetStateVariable("CurrentTrackURI", !currentTrackURI.IsEmpty() ? "" : "Gugus");
      service->SetStateVariable("CurrentTrackURI", currentTrackURI);
      service->SetStateVariable("TransportState", "");
#endif

      service->SetStateVariable("TransportState", media_item_ ? "STOPPED" : "NO_MEDIA_PRESENT");
      service->SetStateVariable("TransportStatus", "OK");

      /* clear time values */
      timeString = PLT_Didl::FormatTimeStamp(0);
      service->SetStateVariable("RelativeTimePosition", timeString);
      service->SetStateVariable("AbsoluteTimePosition", timeString);
      service->SetStateVariable("CurrentTrackDuration", timeString);
      service->SetStateVariable("CurrentMediaDuration", timeString);
    }
  });

  registerHandler<UpdateStateMessage>([this](const UpdateStateMessage&) {
    ML_ENTRY_EXIT();
    UpdateState();
  });
}

/**
 *
 */
MyUPnPRenderer::~MyUPnPRenderer() {
  ML_ENTRY_EXIT();

  std::lock_guard<std::mutex> lock(mutex_);

  media_item_ = nullptr;
  elapsed_time_ = track_time_ = 0;

  renderer_.reset();
}

/**
 *
 */
NPT_Result MyUPnPRenderer::SetupServices() {
  ML_ENTRY_EXIT();

  PLT_Service* rct = NULL;
  PLT_Service* service = NULL;

  NPT_CHECK(PLT_MediaRenderer::SetupServices());

  /* update what we can play */
  NPT_CHECK_FATAL(FindServiceByType("urn:schemas-upnp-org:service:ConnectionManager:1", service));
  service->SetStateVariable("SinkProtocolInfo", RESOURCE_PROTOCOL_INFO_VALUES);

  /* setup mute and value with current values so that any CP sees the current values */
  if (NPT_SUCCEEDED(FindServiceByType("urn:schemas-upnp-org:service:RenderingControl:1", rct))) {
    /* pause automatic eventing, we change multiple state vars */
    EventingPauseGuard guard(rct);

/**
 * WA for Kinsky Volume
 * Kinsky seams to want "channel" and not "Channel", but according
 * UPnP A/V "Channel" would be correct.
 * Tested with other CPs too.
 */
#if 1
    rct->SetStateVariable("Mute", "2");
    rct->SetStateVariableExtraAttribute("Mute", "channel", "Master");
    rct->SetStateVariable("Volume", "999");
    rct->SetStateVariableExtraAttribute("Volume", "channel", "Master");
#endif

    rct->SetStateVariable("Volume", NPT_String::FromInteger(renderer_->getVolume()));
    rct->SetStateVariable("Mute", NPT_String::FromInteger(renderer_->getMute()));
  }

  return NPT_SUCCESS;
}

/**
 *
 */
void MyUPnPRenderer::UpdateStateUnlocked() {
  PLT_Service* service = nullptr;
  const auto state = renderer_->getState();

  if (NPT_FAILED(FindServiceByType("urn:schemas-upnp-org:service:AVTransport:1", service))) {
    return;
  }

  EventingPauseGuard guard(service);

  // -------- Kinsky workaround --------
  {
    NPT_String currentTrackURI;
    service->GetStateVariableValue("CurrentTrackURI", currentTrackURI);

    service->SetStateVariable("CurrentTrackURI", currentTrackURI.IsEmpty() ? "Gugus" : "");
    service->SetStateVariable("CurrentTrackURI", currentTrackURI);
    service->SetStateVariable("TransportState", "");
  }

  // -------- Transport state --------
  switch (state) {
    case RendererState::Stopped: {
      service->SetStateVariable("TransportState", media_item_ ? "STOPPED" : "NO_MEDIA_PRESENT");

      service->SetStateVariable("TransportStatus", "OK");

      NPT_String zeroTime = PLT_Didl::FormatTimeStamp(0);
      service->SetStateVariable("RelativeTimePosition", zeroTime);
      service->SetStateVariable("AbsoluteTimePosition", zeroTime);
      service->SetStateVariable("CurrentTrackDuration", zeroTime);
      service->SetStateVariable("CurrentMediaDuration", zeroTime);
      break;
    }

    case RendererState::Playing:
      service->SetStateVariable("TransportState", "PLAYING");
      service->SetStateVariable("TransportStatus", "OK");
      service->SetStateVariable("TransportPlaySpeed", "1");
      break;

    case RendererState::Paused:
      service->SetStateVariable("TransportState", "PAUSED_PLAYBACK");
      service->SetStateVariable("TransportStatus", "OK");
      break;

    case RendererState::Buffering:
      // Optional: TRANSPORT_STATE = TRANSITIONING
      break;

    default:
      break;
  }
}

/**
 *
 */
void MyUPnPRenderer::UpdateState() {
  ML_ENTRY_EXIT();
  std::lock_guard<std::mutex> lock(mutex_);
  UpdateStateUnlocked();
}

/**
 *
 */
NPT_Result MyUPnPRenderer::OnNext(PLT_ActionReference& action) {
  ML_ENTRY_EXIT();

  /* TODO */
  action = action;

  return NPT_SUCCESS;
}

/**
 *
 */
NPT_Result MyUPnPRenderer::OnPause(PLT_ActionReference& action) {
  ML_ENTRY_EXIT();

  std::lock_guard<std::mutex> lock(mutex_);
  NPT_String instanceID;

  NPT_CHECK_SEVERE(action->GetArgumentValue("InstanceID", instanceID));
  ML_LOG_DEBUG("OnPause InstanceID  %s\n", instanceID.GetChars());

  if (renderer_->getState() == RendererState::Playing) {
    renderer_->pause(this);
  } else if (renderer_->getState() == RendererState::Paused) {
    renderer_->unpause(this);
  }

#if __cplusplus >= 201703L
  send(UpdateStateMessage{});
#else
  send(std::make_shared<UpdateStateMessage>());
#endif

  return NPT_SUCCESS;
}

/**
 *
 */
NPT_Result MyUPnPRenderer::OnPlay(PLT_ActionReference& action) {
  ML_ENTRY_EXIT();

  std::lock_guard<std::mutex> lock(mutex_);
  NPT_Result result = NPT_SUCCESS;
  NPT_String instanceID;
  NPT_String speed;

  NPT_CHECK_SEVERE(action->GetArgumentValue("InstanceID", instanceID));
  ML_LOG_DEBUG("OnPlay InstanceID  %s\n", instanceID.GetChars());

  NPT_CHECK_SEVERE(action->GetArgumentValue("Speed", speed));
  ML_LOG_DEBUG("OnPlay Speed  %s\n", speed.GetChars());

  if (media_item_) {
    if (renderer_->getState() == RendererState::Paused) {
      renderer_->unpause(this);
    } else {
      renderer_->play(this, media_item_);
    }
  } else {
    /* NOP */
  }

#if __cplusplus >= 201703L
  send(UpdateStateMessage{});
#else
  send(std::make_shared<UpdateStateMessage>());
#endif

  return result;
}

/**
 *
 */
NPT_Result MyUPnPRenderer::OnPrevious(PLT_ActionReference& action) {
  ML_ENTRY_EXIT();

  /* TODO */
  action = action;

  return NPT_SUCCESS;
}

/**
 *
 */
NPT_Result MyUPnPRenderer::OnStop(PLT_ActionReference& action) {
  ML_ENTRY_EXIT();

  std::lock_guard<std::mutex> lock(mutex_);
  NPT_String instanceID;

  NPT_CHECK_SEVERE(action->GetArgumentValue("InstanceID", instanceID));
  ML_LOG_DEBUG("OnStop InstanceID  %s\n", instanceID.GetChars());

  /* if playint/pause -> stop */
  renderer_->stop(this);

  elapsed_time_ = track_time_ = 0;

#if __cplusplus >= 201703L
  send(UpdateStateMessage{});
#else
  send(std::make_shared<UpdateStateMessage>());
#endif

  return NPT_SUCCESS;
}

/**
 *
 */
NPT_Result MyUPnPRenderer::OnSetAVTransportURI(PLT_ActionReference& action) {
  ML_ENTRY_EXIT();

  std::lock_guard<std::mutex> lock(mutex_);
  NPT_String currentURI;         /* DLNA URI (from CurrentURI)					*/
  NPT_String currentURIMetaData; /* DLNA meta data (from CurrentURIMetaData)		*/
  NPT_String instanceID;

  NPT_CHECK_SEVERE(action->GetArgumentValue("InstanceID", instanceID));
  ML_LOG_DEBUG("OnSetAVTransportURI InstanceID  %s\n", instanceID.GetChars());

  NPT_CHECK_SEVERE(action->GetArgumentValue("CurrentURI", currentURI));
  ML_LOG_DEBUG("OnSetAVTransportURI CurrentURI  %s\n", currentURI.GetChars());

  NPT_CHECK_SEVERE(action->GetArgumentValue("CurrentURIMetaData", currentURIMetaData));
  ML_LOG_DEBUG("OnSetAVTransportURI CurrentURIMetaData  %s\n", currentURIMetaData.GetChars());

  PLT_Service* service = nullptr;
  elapsed_time_ = track_time_ = 0;

  /**
   * if URI is empty, we have clear the playlist on CP
   * so stop the renderer
   */
  if (currentURI.IsEmpty()) {
    renderer_->stop(this);
    media_item_ = nullptr;

    if (NPT_SUCCEEDED(FindServiceByType("urn:schemas-upnp-org:service:AVTransport:1", service))) {
      EventingPauseGuard guard(service);

      /* clear time values */
      NPT_String time_string = PLT_Didl::FormatTimeStamp(0);
      service->SetStateVariable("RelativeTimePosition", time_string);
      service->SetStateVariable("AbsoluteTimePosition", time_string);
      service->SetStateVariable("CurrentTrackDuration", time_string);
      service->SetStateVariable("CurrentMediaDuration", time_string);

      service->SetStateVariable("TransportState", "NO_MEDIA_PRESENT");
      service->SetStateVariable("TransportStatus", "OK");

      service->SetStateVariable("NumberOfTracks", "0");
      service->SetStateVariable("CurrentTrack", "0");
      service->SetStateVariable("AVTransportURI", "");
      service->SetStateVariable("CurrentTrackURI", "");
      service->SetStateVariable("AVTransportURIMetaData", "");
      service->SetStateVariable("CurrentTrackMetadata", "");
    }
    return NPT_SUCCESS;
  } else {
    media_item_ = std::make_shared<MediaItem>();
    media_item_->uri = NPT_Uri::PercentDecode(currentURI);

    if (NPT_SUCCEEDED(FindServiceByType("urn:schemas-upnp-org:service:AVTransport:1", service))) {
      EventingPauseGuard guard(service);

      /* clear time values */
      NPT_String time_string = PLT_Didl::FormatTimeStamp(0);
      service->SetStateVariable("RelativeTimePosition", time_string);
      service->SetStateVariable("AbsoluteTimePosition", time_string);
      service->SetStateVariable("CurrentTrackDuration", time_string);
      service->SetStateVariable("CurrentMediaDuration", time_string);

      service->SetStateVariable("NumberOfTracks", "1");
      service->SetStateVariable("CurrentTrack", "1");
      service->SetStateVariable("AVTransportURI", currentURI);
      service->SetStateVariable("CurrentTrackURI", currentURI);
      service->SetStateVariable("AVTransportURIMetaData", currentURIMetaData);
      service->SetStateVariable("CurrentTrackMetadata", currentURIMetaData);
    }
    return NPT_SUCCESS;
  }
}

/**
 *
 */
NPT_Result MyUPnPRenderer::OnSetVolume(PLT_ActionReference& action) {
  ML_ENTRY_EXIT();

  std::lock_guard<std::mutex> lock(mutex_);
  NPT_String instanceID;
  NPT_String channel;
  NPT_String desiredVolume;
  int volume;

  NPT_CHECK_SEVERE(action->GetArgumentValue("InstanceID", instanceID));
  ML_LOG_DEBUG("OnSetVolume InstanceID  %s\n", instanceID.GetChars());

  NPT_CHECK_SEVERE(action->GetArgumentValue("Channel", channel));
  ML_LOG_DEBUG("OnSetVolume Channel  %s\n", channel.GetChars());

  NPT_CHECK_SEVERE(action->GetArgumentValue("DesiredVolume", desiredVolume));
  ML_LOG_DEBUG("OnSetVolume DesiredVolume  %s\n", desiredVolume.GetChars());

  if (channel.Compare("Master") != 0) {
    action->SetError(800, "Internal error");
    return NPT_FAILURE;
  }

  if (desiredVolume.ToInteger32(volume) != NPT_SUCCESS) {
    action->SetError(800, "Internal error");
    return NPT_FAILURE;
  }

  ML_LOG_DEBUG("OnSetVolume volume [%d]\n", volume);

  if (volume < 0 || volume > 100) {
    action->SetError(800, "Internal error");
    return NPT_FAILURE;
  }

  renderer_->setVolume(this, volume);

  return NPT_SUCCESS;
}

/**
 *
 */
NPT_Result MyUPnPRenderer::OnGetVolumeDBRange(PLT_ActionReference& action) {
  ML_ENTRY_EXIT();

  /* TODO */
  action = action;

  return NPT_SUCCESS;
}

/**
 *
 */
NPT_Result MyUPnPRenderer::OnSetMute(PLT_ActionReference& action) {
  ML_ENTRY_EXIT();

  std::lock_guard<std::mutex> lock(mutex_);
  NPT_String instanceID;
  NPT_String channel;
  NPT_String desiredMute;
  int mute;

  NPT_CHECK_SEVERE(action->GetArgumentValue("InstanceID", instanceID));
  ML_LOG_DEBUG("OnSetMute InstanceID  %s\n", instanceID.GetChars());

  NPT_CHECK_SEVERE(action->GetArgumentValue("Channel", channel));
  ML_LOG_DEBUG("OnSetMute Channel  %s\n", channel.GetChars());

  NPT_CHECK_SEVERE(action->GetArgumentValue("DesiredMute", desiredMute));
  ML_LOG_DEBUG("OnSetMute DesiredMute  %s\n", desiredMute.GetChars());

  if (channel.Compare("Master") != 0) {
    action->SetError(800, "Internal error");
    return NPT_FAILURE;
  }

  if (!desiredMute.IsEmpty()) {
    if (desiredMute[0] == 'F' || desiredMute[0] == '0') {
      mute = 0;
    } else if (desiredMute[0] == 'T' || desiredMute[0] == '1') {
      mute = 1;
    } else {
      action->SetError(800, "Internal error");
      return NPT_FAILURE;
    }

    renderer_->setMute(this, mute);
  }

  return NPT_SUCCESS;
}

/**
 *
 */
NPT_Result MyUPnPRenderer::OnSeek(PLT_ActionReference& action) {
  ML_ENTRY_EXIT();

  std::lock_guard<std::mutex> lock(mutex_);
  NPT_String instanceID;
  NPT_String unit;
  NPT_String target;
  NPT_UInt32 time;

  NPT_CHECK_SEVERE(action->GetArgumentValue("InstanceID", instanceID));
  ML_LOG_DEBUG("OnSeek InstanceID  %s\n", instanceID.GetChars());

  NPT_CHECK_SEVERE(action->GetArgumentValue("Unit", unit));
  ML_LOG_DEBUG("OnSeek Unit  %s\n", unit.GetChars());

  NPT_CHECK_SEVERE(action->GetArgumentValue("Target", target));
  ML_LOG_DEBUG("OnSeek Target  %s\n", target.GetChars());

  /**
   * Note that ABS_TIME and REL_TIME don't mean what you'd think
   * they mean.  REL_TIME means relative to the current track,
   * ABS_TIME to the whole media (ie for a multitrack tape). So
   * take both ABS and REL as absolute position in the current song
   */

  if ((unit.Compare("REL_TIME") == 0) || (unit.Compare("ABS_TIME") == 0)) {
    /* converts target to seconds */
    NPT_CHECK_SEVERE(PLT_Didl::ParseTimeStamp(target, time));
  } else {
    action->SetError(800, "Internal error");
    return NPT_FAILURE;
  }

  if (unit.Compare("REL_TIME") == 0) {
    ML_LOG_DEBUG("seek REL to [%u] seconds\n", time);
    /* even named REL_TIME it is an absolut seek */
    renderer_->seek(this, 0, time);
  } else if (unit.Compare("ABS_TIME") == 0) {
    ML_LOG_DEBUG("seek ABS to [%u] seconds\n", time);
    renderer_->seek(this, 0, time);
  }

  return NPT_SUCCESS;
}

/**
 *
 */
void MyUPnPRenderer::RendererChanges(SynchronizedStatus* status) {
  ML_ENTRY_EXIT();
  PLT_Service* service = nullptr;
  /* setup mute and value with current values so that any CP sees the current values */
  if (NPT_SUCCEEDED(FindServiceByType("urn:schemas-upnp-org:service:RenderingControl:1", service))) {
    EventingPauseGuard guard(service);

    service->SetStateVariable("Volume", NPT_String::FromInteger(status->volume));
    service->SetStateVariable("Mute", NPT_String::FromInteger(status->mute));
  }
}
