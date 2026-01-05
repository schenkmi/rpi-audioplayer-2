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

/* Platinum/Neptune UPnP SDK includes */
#include <PltDidl.h>
#include <PltService.h>
#include <PltUtilities.h>
/* local includes */
#include "MyLogger.h"
#include "MyOHPlaylist.h"
#include "Renderer.h"

#include <assert.h>
#include <algorithm>
#include <unordered_map>

NPT_SET_LOCAL_LOGGER("platinum.oh.myplaylist")

/**
 *
 */
MyOHPlaylist::MyOHPlaylist(std::shared_ptr<IRenderer> renderer, const char* friendly_name, bool show_ip, const char* uuid, unsigned int port)
    : IMyPLTController(renderer), PLT_OHPlaylist(friendly_name, show_ip, uuid, port, true), index_(-1), room_(friendly_name), id_(0), token_(1), id_array_("") {
  ML_ENTRY_EXIT();

  renderer_->registerNotifier(this);

  registerHandler<UpdatePlayTimeMessage>([this](const UpdatePlayTimeMessage& msg) {
    ML_ENTRY_EXIT();

    ML_LOG_TRACE(" UpdatePlayTimeMessage time=%u, duration=%u\n", msg.time, msg.duration);

    std::lock_guard<std::mutex> lock(mutex_);

    elapsed_time_ = msg.time;
    track_time_ = msg.duration;
    PLT_Service* service = nullptr;

    if (NPT_SUCCEEDED(FindServiceByType("urn:av-openhome-org:service:Time:1", service))) {
      EventingPauseGuard guard(service);
      service->SetStateVariable("Seconds", NPT_String::FromIntegerU(elapsed_time_));
      service->SetStateVariable("Duration", NPT_String::FromIntegerU(track_time_));
    }

    if (NPT_SUCCEEDED(FindServiceByType("urn:av-openhome-org:service:Info:1", service))) {
      EventingPauseGuard guard(service);
      service->SetStateVariable("Duration", NPT_String::FromIntegerU(track_time_));
    }
  });

  registerHandler<PlayNextMessage>([this](const PlayNextMessage&) {
    ML_ENTRY_EXIT();

    std::lock_guard<std::mutex> lock(mutex_);

    const size_t size = media_items_.size();
    if (size == 0 || index_ == -1) {
      return;
    }

    if (renderer_->getShuffle()) {
      // Pick a random index different from the current one if possible
      if (size > 1) {
        int next;
        do {
          next = rand() % size;
        } while (next == index_);
        index_ = next;
      } else {
        index_ = -1;
      }
    } else {
      // Sequential mode
      if (index_ + 1 < (int)size) {
        ++index_;
      } else if (renderer_->getRepeat()) {
        index_ = 0;
      } else {
        // end of playlist, stop
        index_ = -1;
      }
    }

    elapsed_time_ = track_time_ = 0;

    if (index_ == -1) {
      renderer_->stop(this);
    } else {
      renderer_->play(this, media_items_[index_]);
    }

#if __cplusplus >= 201703L
    send(UpdateStateMessage{});
#else
    send(std::make_shared<UpdateStateMessage>());
#endif
  });

  registerHandler<UpdateStateMessage>([this](const UpdateStateMessage&) {
    ML_ENTRY_EXIT();
    UpdateState();
  });
}

/**
 *
 */
MyOHPlaylist::~MyOHPlaylist() {
  ML_ENTRY_EXIT();

  std::lock_guard<std::mutex> lock(mutex_);

  // clear media_items_ and free the memory
  std::vector<std::shared_ptr<MediaItem>>{}.swap(media_items_);

  index_ = -1;
  elapsed_time_ = track_time_ = 0;

  renderer_.reset();
}

/**
 *
 */
void MyOHPlaylist::UpdateStateUnlocked() {
  const bool hasTrack = (index_ >= 0 && index_ < static_cast<int>(media_items_.size()));

  const auto rendererState = renderer_->getState();
  const auto index = index_;
  const auto items = media_items_; // snapshot
  const auto token = token_;
  const auto idArray = id_array_;
  const auto elapsed = elapsed_time_;
  PLT_Service* service = nullptr;

  // -------- Playlist Service --------
  if (NPT_SUCCEEDED(FindServiceByType("urn:av-openhome-org:service:Playlist:1", service))) {
    EventingPauseGuard guard(service);

    service->SetStateVariable("Id", hasTrack ? NPT_String::FromIntegerU(items[index]->ohPltID) : "0");
    service->SetStateVariable("IdArray", idArray);
    service->SetStateVariable("IdArrayToken", NPT_String::FromIntegerU(token));

    switch (rendererState) {
      case RendererState::Stopped:
        service->SetStateVariable("TransportState", "Stopped");
        break;
      case RendererState::Playing:
        service->SetStateVariable("TransportState", "Playing");
        break;
      case RendererState::Paused:
        service->SetStateVariable("TransportState", "Paused");
        break;
      case RendererState::Buffering:
        service->SetStateVariable("TransportState", "Buffering");
        break;
      default:
        break;
    }
  }

  // -------- Info Service --------
  if (NPT_SUCCEEDED(FindServiceByType("urn:av-openhome-org:service:Info:1", service))) {
    EventingPauseGuard guard(service);

    if (hasTrack) {
      const auto& item = items[index];

      service->SetStateVariable("Uri", item->uri.c_str());
      service->SetStateVariable("Metadata", item->ohPltMetadata.c_str());
      service->SetStateVariable("TrackCount", NPT_String::FromIntegerU(items.size()));

      if (item->duration != 0) {
        service->SetStateVariable("Duration", NPT_String::FromIntegerU(item->duration));
      }
      auto meta = item->getMetaData();
      if (meta && !meta->resources.empty()) {
        const auto& r = meta->resources.front();
        service->SetStateVariable("BitRate", NPT_String::FromInteger(r.bitrate));
        service->SetStateVariable("BitDepth", NPT_String::FromInteger(r.bitsPerSample));
        service->SetStateVariable("SampleRate", NPT_String::FromInteger(r.sampleFrequency));
      }
    } else {
      NPT_String empty_didl = R"(
        <DIDL-Lite
        xmlns="urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/"
        xmlns:dc="http://purl.org/dc/elements/1.1/"
        xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">
        <item id="0" parentID="0" restricted="true">
        <dc:title></dc:title>
        <upnp:class>object.item.audioItem</upnp:class>
        </item>
        </DIDL-Lite>
      )";

      // unchanged default block
      service->SetStateVariable("TrackCount", "0");
      service->SetStateVariable("DetailsCount", "0");
      service->SetStateVariable("MetatextCount", "0");
      service->SetStateVariable("Uri", "");
      service->SetStateVariable("Metadata", empty_didl);
      service->SetStateVariable("Duration", "0");
      service->SetStateVariable("BitRate", "0");
      service->SetStateVariable("BitDepth", "0");
      service->SetStateVariable("SampleRate", "0");
      service->SetStateVariable("Lossless", "0");
      service->SetStateVariable("CodecName", "");
      service->SetStateVariable("Metatext", "");
    }
  }

  // -------- Time Service --------
  if (NPT_SUCCEEDED(FindServiceByType("urn:av-openhome-org:service:Time:1", service))) {
    EventingPauseGuard guard(service);

    if (hasTrack) {
      service->SetStateVariable("TrackCount", NPT_String::FromIntegerU(items.size()));

      if (items[index]->duration != 0) {
        service->SetStateVariable("Duration", NPT_String::FromIntegerU(items[index]->duration));
      }

      service->SetStateVariable("Seconds", NPT_String::FromIntegerU(elapsed));
    } else {
      service->SetStateVariable("TrackCount", "0");
      service->SetStateVariable("Duration", "0");
      service->SetStateVariable("Seconds", "0");
    }
  }
}

void MyOHPlaylist::UpdateState() {
  ML_ENTRY_EXIT();
  std::lock_guard<std::mutex> lock(mutex_);
  UpdateStateUnlocked();
}

/**
 *
 */
NPT_Result MyOHPlaylist::SetupServices() {
  ML_ENTRY_EXIT();

  NPT_CHECK(PLT_OHPlaylist::SetupServices());

  PLT_Service* service = nullptr;

  if (NPT_SUCCEEDED(FindServiceByType("urn:av-openhome-org:service:Playlist:1", service))) {
    /* pause automatic eventing, we change multiple state vars */
    EventingPauseGuard guard(service);

    service->SetStateVariable("ProtocolInfo", RESOURCE_PROTOCOL_INFO_VALUES);
    service->SetStateVariable("Shuffle", NPT_String::FromInteger(renderer_->getShuffle()));
    service->SetStateVariable("Repeat", NPT_String::FromInteger(renderer_->getRepeat()));
  }

  if (NPT_SUCCEEDED(FindServiceByType("urn:av-openhome-org:service:Product:1", service))) {
    EventingPauseGuard guard(service);

    service->SetStateVariable("ProductRoom", room_.c_str());
  }

  if (NPT_SUCCEEDED(FindServiceByType("urn:av-openhome-org:service:Volume:1", service))) {
    EventingPauseGuard guard(service);

    service->SetStateVariable("Volume", NPT_String::FromInteger(renderer_->getVolume()));
    service->SetStateVariable("Mute", NPT_String::FromInteger(renderer_->getMute()));
  }

  return NPT_SUCCESS;
}

/**
 *
 */
void MyOHPlaylist::CreateIdArray(NPT_String& id_array) {
  ML_ENTRY_EXIT();

  id_array = "";

  if (media_items_.empty()) return;

  const size_t count = media_items_.size();
  std::vector<NPT_Byte> bytes;
  bytes.reserve(count * 4);

  for (const auto& item : media_items_) {
    const uint32_t id = item->ohPltID;
    bytes.push_back(static_cast<NPT_Byte>((id >> 24) & 0xFF));
    bytes.push_back(static_cast<NPT_Byte>((id >> 16) & 0xFF));
    bytes.push_back(static_cast<NPT_Byte>((id >> 8) & 0xFF));
    bytes.push_back(static_cast<NPT_Byte>(id & 0xFF));
  }

  NPT_Base64::Encode(bytes.data(), static_cast<NPT_Size>(bytes.size()), id_array);
}

/**
 *
 */
NPT_Result MyOHPlaylist::OnPlaylistInsert(PLT_ActionReference& action) {
  ML_ENTRY_EXIT();

  std::lock_guard<std::mutex> lock(mutex_);

  NPT_String afterIdString, uri, meta;
  NPT_UInt32 afterId = 0;

  NPT_CHECK_SEVERE(action->GetArgumentValue("AfterId", afterIdString));
  ML_LOG_DEBUG("OnPlaylistInsert AfterId %s\n", afterIdString.GetChars());
  NPT_CHECK_SEVERE(afterIdString.ToInteger32(afterId));

  NPT_CHECK_SEVERE(action->GetArgumentValue("Uri", uri));
  ML_LOG_DEBUG("OnPlaylistInsert Uri %s\n", uri.GetChars());

  NPT_CHECK_SEVERE(action->GetArgumentValue("Metadata", meta));
  ML_LOG_TRACE("OnPlaylistInsert Metadata %s\n", meta.GetChars());

  if (meta.IsEmpty()) return NPT_SUCCESS;

  /* Find insertion point */
  auto it = media_items_.begin();

  if (afterId != 0) {
    it = std::find_if(media_items_.begin(), media_items_.end(), [&](const std::shared_ptr<MediaItem>& item) { return item->ohPltID == afterId; });

    if (it == media_items_.end()) {
      ML_LOG_DEBUG("AfterId %d not found\n", afterId);
      return NPT_ERROR_NOT_IMPLEMENTED;
    }

    ++it; // insert *after*
  }

  auto item = std::make_shared<MediaItem>();
  item->uri = NPT_Uri::PercentDecode(uri);

  /* Assign new ID */
  item->origin = MediaItemOrigin::OpenHome;
  item->ohPltID = ++id_;
  item->ohPltMetadata = meta.GetChars();

  media_items_.insert(it, item);

  ML_LOG_DEBUG("-> ID %u\n", item->ohPltID);

  PLT_Service* service = nullptr;
  /* Update state variables */
  if (NPT_SUCCEEDED(FindServiceByType("urn:av-openhome-org:service:Playlist:1", service))) {
    EventingPauseGuard guard(service);

    CreateIdArray(id_array_);
    service->SetStateVariable("IdArray", id_array_);

    service->SetStateVariable("IdArrayToken", NPT_String::FromIntegerU(++token_));
  }

  action->SetArgumentValue("NewId", NPT_String::FromIntegerU(item->ohPltID));

  return NPT_SUCCESS;
}

/**
 *
 */

NPT_Result MyOHPlaylist::OnPlaylistIdArray(PLT_ActionReference& action) {
  ML_ENTRY_EXIT();

  std::lock_guard<std::mutex> lock(mutex_);

  CreateIdArray(id_array_);

  NPT_CHECK_SEVERE(action->SetArgumentValue("Array", id_array_));

  action->SetArgumentValue("Token", NPT_String::FromIntegerU(++token_));

  return NPT_SUCCESS;
}

/**
 *
 */
NPT_Result MyOHPlaylist::OnPlaylistDeleteAll(PLT_ActionReference& action) {
  ML_ENTRY_EXIT();

  std::lock_guard<std::mutex> lock(mutex_);

  if (renderer_) renderer_->stop(this);

  /* clear playlist and free memory */
  std::vector<std::shared_ptr<MediaItem>>{}.swap(media_items_);

  index_ = -1;
  elapsed_time_ = track_time_ = 0;

  CreateIdArray(id_array_);

  ++token_;

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
NPT_Result MyOHPlaylist::OnPlaylistDeleteId(PLT_ActionReference& action) {
  ML_ENTRY_EXIT();

  std::lock_guard<std::mutex> lock(mutex_);

  NPT_String id;
  uint32_t idValue = 0;

  NPT_CHECK_SEVERE(action->GetArgumentValue("Value", id));
  ML_LOG_DEBUG("OnPlaylistDeleteId Id = %s\n", id.GetChars());
  NPT_CHECK_SEVERE(id.ToInteger32(idValue));

  int currentPltID = (index_ >= 0 && index_ < (int)media_items_.size()) ? media_items_[index_]->ohPltID : -1;

  auto it = std::find_if(media_items_.begin(), media_items_.end(), [&](const std::shared_ptr<MediaItem>& item) { return item->ohPltID == idValue; });

  if (it != media_items_.end()) {
    const int erasedIndex = std::distance(media_items_.begin(), it);
    media_items_.erase(it);

    /* deleted currently active item */
    if (erasedIndex == index_) {
      // keep it simple and just stop
      elapsed_time_ = track_time_ = 0;
      currentPltID = -1;
      renderer_->stop(this);
    }
  }

  /* recompute index */
  index_ = -1;
  if (currentPltID != -1) {
    for (size_t i = 0; i < media_items_.size(); ++i) {
      if (media_items_[i]->ohPltID == currentPltID) {
        index_ = static_cast<int>(i);
        break;
      }
    }
  }

  token_++;
  CreateIdArray(id_array_);

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
NPT_Result MyOHPlaylist::OnPlaylistReadList(PLT_ActionReference& action) {
  ML_ENTRY_EXIT();

  std::lock_guard<std::mutex> lock(mutex_);

  NPT_String idList;
  NPT_CHECK_SEVERE(action->GetArgumentValue("IdList", idList));
  ML_LOG_DEBUG("OnPlaylistReadList IdList %s\n", idList.GetChars());

  /* Build lookup table (order-neutral) */
  std::unordered_map<NPT_Int32, std::shared_ptr<MediaItem>> lookup;
  lookup.reserve(media_items_.size());
  for (const auto& item : media_items_) lookup[item->ohPltID] = item;

  /* Split list — must keep this alive */
  NPT_List<NPT_String> ids = idList.Split(" ");

  NPT_String csxml;
  csxml.Reserve(256 + ids.GetItemCount() * 128);
  csxml = "<TrackList>";

  /* Iterate in IdList order */
  for (auto it = ids.GetFirstItem(); it; ++it) {
    NPT_Int32 id;
    if (NPT_FAILED(it->ToInteger32(id))) continue;

    auto found = lookup.find(id);
    if (found == lookup.end()) continue;

    const auto& item = found->second;

    csxml += "<Entry>";

    csxml += "<Id>";
    csxml += NPT_String::FromIntegerU(item->ohPltID);
    csxml += "</Id>";

    csxml += "<Uri>";
    PLT_Didl::AppendXmlEscape(csxml, item->uri.c_str());
    csxml += "</Uri>";

    csxml += "<Metadata>";
    PLT_Didl::AppendXmlEscape(csxml, item->ohPltMetadata.c_str());
    csxml += "</Metadata>";

    csxml += "</Entry>";
  }

  csxml += "</TrackList>";

  ML_LOG_TRACE("TrackList %s\n", csxml.GetChars());
  NPT_CHECK_SEVERE(action->SetArgumentValue("TrackList", csxml));

  return NPT_SUCCESS;
}

/**
 * Index is from start of current playlist, not id dependend
 */
NPT_Result MyOHPlaylist::OnPlaylistSeekIndex(PLT_ActionReference& action) {
  ML_ENTRY_EXIT();

  std::lock_guard<std::mutex> lock(mutex_);

  NPT_String value;
  NPT_CHECK_SEVERE(action->GetArgumentValue("Value", value));
  ML_LOG_DEBUG("OnPlaylistSeekIndex SeekIndex value %s\n", value.GetChars());

  NPT_Int32 index = -1;
  if (NPT_FAILED(value.ToInteger32(index)) || index < 0 || static_cast<size_t>(index) >= media_items_.size()) {
    index_ = -1;
  } else {
    index_ = index;

    if (const auto& item = media_items_[index_]) {
      renderer_->play(this, item);
    } else {
      index_ = -1;
    }
  }

#if __cplusplus >= 201703L
  send(UpdateStateMessage{});
#else
  send(std::make_shared<UpdateStateMessage>());
#endif

  return NPT_SUCCESS;
}

/**
 * Index is from start of current playlist, not id dependend
 */
NPT_Result MyOHPlaylist::OnPlaylistSeekId(PLT_ActionReference& action) {
  ML_ENTRY_EXIT();

  std::lock_guard<std::mutex> lock(mutex_);

  NPT_String value;
  NPT_CHECK_SEVERE(action->GetArgumentValue("Value", value));
  ML_LOG_DEBUG("OnPlaylistSeekId ID = %s\n", value.GetChars());

  NPT_Int32 id = 0;
  NPT_CHECK_SEVERE(value.ToInteger32(id));

  auto it = std::find_if(media_items_.begin(), media_items_.end(), [&](const std::shared_ptr<MediaItem>& item) { return item && item->ohPltID == id; });

  if (it == media_items_.end()) {
    action->SetError(401, "Id not found");
    return NPT_FAILURE;
  }

  index_ = static_cast<int>(std::distance(media_items_.begin(), it));

  if (const auto& item = *it) {
    renderer_->play(this, item);
  } else {
    action->SetError(401, "Invalid media item");
    return NPT_FAILURE;
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
NPT_Result MyOHPlaylist::OnPlaylistPlay(PLT_ActionReference& action) {
  ML_ENTRY_EXIT();

  std::lock_guard<std::mutex> lock(mutex_);

  if (index_ < 0 || static_cast<size_t>(index_) >= media_items_.size()) {
    action->SetError(401, "Id not found");
    return NPT_FAILURE;
  }

  const auto state = renderer_->getState();

  if (state == RendererState::Paused) {
    renderer_->unpause(this);
  } else if (state == RendererState::Stopped) {
    if (const auto& item = media_items_[index_]) {
      renderer_->play(this, item);
    } else {
      action->SetError(401, "Invalid media item");
      return NPT_FAILURE;
    }
  }
  // If already Playing, do nothing (idempotent)

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
NPT_Result MyOHPlaylist::OnPlaylistPause(PLT_ActionReference& action) {
  ML_ENTRY_EXIT();

  std::lock_guard<std::mutex> lock(mutex_);

  const auto state = renderer_->getState();

  if (state == RendererState::Playing) {
    renderer_->pause(this);

  } else if (state == RendererState::Paused) {
    renderer_->unpause(this);
  }
  // Stopped -> no-op

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
NPT_Result MyOHPlaylist::OnPlaylistStop(PLT_ActionReference& action) {
  ML_ENTRY_EXIT();

  std::lock_guard<std::mutex> lock(mutex_);

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
NPT_Result MyOHPlaylist::OnPlaylistNext(PLT_ActionReference& action) {
  ML_ENTRY_EXIT();

  std::lock_guard<std::mutex> lock(mutex_);

  const size_t size = media_items_.size();
  if (size == 0 || index_ == -1) {
    action->SetError(401, "Id not found");
    return NPT_FAILURE;
  }

  if (renderer_->getShuffle()) {
    // Pick a random index different from the current one if possible
    if (size > 1) {
      int next;
      do {
        next = rand() % size;
      } while (next == index_);
      index_ = next;
    }
  } else {
    // Sequential mode
    if (index_ + 1 < (int)size) {
      ++index_;
    } else if (renderer_->getRepeat()) {
      index_ = 0;
    } else {
      action->SetError(401, "End of playlist");
      return NPT_FAILURE;
    }
  }

  elapsed_time_ = track_time_ = 0;

  renderer_->play(this, media_items_[index_]);

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
NPT_Result MyOHPlaylist::OnPlaylistPrevious(PLT_ActionReference& action) {
  ML_ENTRY_EXIT();

  std::lock_guard<std::mutex> lock(mutex_);

  const size_t size = media_items_.size();
  if (size == 0 || index_ == -1) {
    action->SetError(401, "Id not found");
    return NPT_FAILURE;
  }

  if (renderer_->getShuffle()) {
    // Random previous (same semantics as Next in shuffle)
    if (size > 1) {
      int prev;
      do {
        prev = rand() % size;
      } while (prev == index_);
      index_ = prev;
    }
  } else {
    // Sequential mode
    if (index_ > 0) {
      --index_;
    } else if (renderer_->getRepeat()) {
      index_ = static_cast<int>(size) - 1;
    } else {
      action->SetError(401, "Start of playlist");
      return NPT_FAILURE;
    }
  }

  elapsed_time_ = track_time_ = 0;

  renderer_->play(this, media_items_[index_]);

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
NPT_Result MyOHPlaylist::OnPlaylistSeekSecondAbsolute(PLT_ActionReference& action) {
  ML_ENTRY_EXIT();

  std::lock_guard<std::mutex> lock(mutex_);

  if (index_ == -1 || media_items_.empty()) {
    action->SetError(401, "No active track");
    return NPT_FAILURE;
  }

  NPT_String value;
  NPT_UInt32 time = 0;

  NPT_CHECK_SEVERE(action->GetArgumentValue("Value", value));
  ML_LOG_DEBUG("OnPlaylistSeekSecondAbsolute Value = %s\n", value.GetChars());
  NPT_CHECK_SEVERE(value.ToInteger32(time));

  renderer_->seek(this, 0, time);

  return NPT_SUCCESS;
}

/**
 *
 */
NPT_Result MyOHPlaylist::OnPlaylistSeekSecondRelative(PLT_ActionReference& action) {
  ML_ENTRY_EXIT();

  std::lock_guard<std::mutex> lock(mutex_);

  if (index_ == -1 || media_items_.empty()) {
    action->SetError(401, "No active track");
    return NPT_FAILURE;
  }

  NPT_String value;
  NPT_Int32 delta = 0;

  NPT_CHECK_SEVERE(action->GetArgumentValue("Value", value));
  ML_LOG_DEBUG("OnPlaylistSeekSecondRelative Value = %s\n", value.GetChars());
  NPT_CHECK_SEVERE(value.ToInteger32(delta));

  renderer_->seek(this, 1, delta);

  return NPT_SUCCESS;
}

/**
 *
 */

NPT_Result MyOHPlaylist::OnPlaylistSetRepeat(PLT_ActionReference& action) {
  ML_ENTRY_EXIT();

  std::lock_guard<std::mutex> lock(mutex_);
  NPT_String value;
  int repeat;

  NPT_CHECK_SEVERE(action->GetArgumentValue("Value", value));
  ML_LOG_DEBUG("OnPlaylistSetRepeat Value = %s\n", value.GetChars());

  NPT_CHECK_SEVERE(value.ToInteger32(repeat));

  renderer_->setRepeat(this, repeat);

  return NPT_SUCCESS;
}

/**
 *
 */
NPT_Result MyOHPlaylist::OnPlaylistRepeat(PLT_ActionReference& action) {
  ML_ENTRY_EXIT();

  action->SetArgumentValue("Value", NPT_String::FromInteger(renderer_->getRepeat()));

  return NPT_SUCCESS;
}

/**
 *
 */
NPT_Result MyOHPlaylist::OnPlaylistSetShuffle(PLT_ActionReference& action) {
  ML_ENTRY_EXIT();

  std::lock_guard<std::mutex> lock(mutex_);
  NPT_String value;
  int shuffle;

  NPT_CHECK_SEVERE(action->GetArgumentValue("Value", value));
  ML_LOG_DEBUG("OnPlaylistSetShuffle Value = %s\n", value.GetChars());

  NPT_CHECK_SEVERE(value.ToInteger32(shuffle));

  renderer_->setShuffle(this, shuffle);

  return NPT_SUCCESS;
}

/**
 *
 */
NPT_Result MyOHPlaylist::OnPlaylistShuffle(PLT_ActionReference& action) {
  ML_ENTRY_EXIT();

  action->SetArgumentValue("Value", NPT_String::FromInteger(renderer_->getShuffle()));

  return NPT_SUCCESS;
}

/**
 *
 */
NPT_Result MyOHPlaylist::OnPlaylistSetVolume(PLT_ActionReference& action) {
  ML_ENTRY_EXIT();

  std::lock_guard<std::mutex> lock(mutex_);
  NPT_String value;
  int volume;

  NPT_CHECK_SEVERE(action->GetArgumentValue("Value", value));
  ML_LOG_DEBUG("OnPlaylistSetVolume Value = %s\n", value.GetChars());

  NPT_CHECK_SEVERE(value.ToInteger32(volume));

  renderer_->setVolume(this, volume);

  return NPT_SUCCESS;
}

/**
 *
 */
NPT_Result MyOHPlaylist::OnPlaylistVolume(PLT_ActionReference& action) {
  ML_ENTRY_EXIT();

  action->SetArgumentValue("Value", NPT_String::FromInteger(renderer_->getVolume()));

  return NPT_SUCCESS;
}

/**
 *
 */
NPT_Result MyOHPlaylist::OnPlaylistVolumeInc(PLT_ActionReference& action __attribute__((unused))) {
  ML_ENTRY_EXIT();

  std::lock_guard<std::mutex> lock(mutex_);
  int volume = renderer_->getVolume();

  if (volume < 100) {
    volume++;
    renderer_->setVolume(this, volume);
  }

  return NPT_SUCCESS;
}

/**
 *
 */
NPT_Result MyOHPlaylist::OnPlaylistVolumeDec(PLT_ActionReference& action __attribute__((unused))) {
  ML_ENTRY_EXIT();

  std::lock_guard<std::mutex> lock(mutex_);
  int volume = renderer_->getVolume();

  if (volume > 0) {
    volume--;
    renderer_->setVolume(this, volume);
  }

  return NPT_SUCCESS;
}

/**
 *
 */
NPT_Result MyOHPlaylist::OnPlaylistSetMute(PLT_ActionReference& action) {
  ML_ENTRY_EXIT();

  std::lock_guard<std::mutex> lock(mutex_);
  NPT_String value;
  int mute;

  NPT_CHECK_SEVERE(action->GetArgumentValue("Value", value));
  ML_LOG_DEBUG("OnPlaylistSetMute Value = %s\n", value.GetChars());

  NPT_CHECK_SEVERE(value.ToInteger32(mute));

  renderer_->setMute(this, mute);

  return NPT_SUCCESS;
}

/**
 *
 */
NPT_Result MyOHPlaylist::OnPlaylistMute(PLT_ActionReference& action) {
  ML_ENTRY_EXIT();

  action->SetArgumentValue("Value", NPT_String::FromInteger(renderer_->getMute()));

  return NPT_SUCCESS;
}

/**
 *
 */
void MyOHPlaylist::RendererChanges(SynchronizedStatus* status) {
  ML_ENTRY_EXIT();
  PLT_Service* service = nullptr;
  if (NPT_SUCCEEDED(FindServiceByType("urn:av-openhome-org:service:Volume:1", service))) {
    EventingPauseGuard guard(service);

    service->SetStateVariable("Volume", NPT_String::FromInteger(status->volume));
    service->SetStateVariable("Mute", NPT_String::FromInteger(status->mute));
  }

  if (NPT_SUCCEEDED(FindServiceByType("urn:av-openhome-org:service:Playlist:1", service))) {
    EventingPauseGuard guard(service);

    service->SetStateVariable("Repeat", NPT_String::FromInteger(status->repeat));
    service->SetStateVariable("Shuffle", NPT_String::FromInteger(status->shuffle));
  }
}
