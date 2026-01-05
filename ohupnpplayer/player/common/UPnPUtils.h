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

#include <memory>
#include <string>
#include <vector>
/* platinum includes */
#include <PltMediaItem.h>
#include <PltUPnP.h>
/* local includes */
#include <MediaItem.h>

/**
 * Create MediaItem from Platinum PLT_MediaObject
 */
static inline std::shared_ptr<MediaItem> CreateMediaItemFromMediaObjectMinimalistic(PLT_MediaObject* mediaObject) {
  const auto res = mediaObject->m_Resources.GetFirstItem();
  if (res == nullptr) {
    return {};
  }
  auto item = std::make_shared<MediaItem>();

  item->uri = NPT_Uri::PercentDecode(res->m_Uri.GetChars());
  item->duration = (res->m_Duration != (NPT_UInt32)-1) ? res->m_Duration : 0;

  return item;
}

/**
 * Create MetaData from Platinum PLT_MediaObject
 */
static inline std::shared_ptr<MetaData> create_metadata_from_media_object(PLT_MediaObject* mediaObject) {
  if (!mediaObject) return nullptr;

  auto meta = std::make_shared<MetaData>();
  NPT_String decodeURI;

  // Type
  if (mediaObject->IsContainer()) {
    meta->type = kMetaDataTypeContainer;
  } else if (mediaObject->m_ObjectClass.type.StartsWith("object.item.audioItem")) {
    meta->type = kMetaDataTypeAudio;
  } else if (mediaObject->m_ObjectClass.type.StartsWith("object.item.imageItem")) {
    meta->type = kMetaDataTypeImage;
  } else if (mediaObject->m_ObjectClass.type.StartsWith("object.item.videoItem")) {
    meta->type = kMetaDataTypeVideo;
  } else {
    meta->type = kMetaDataTypeUnknown;
  }

  // Basic metadata
  meta->title = mediaObject->m_Title.GetChars();
  meta->creator = mediaObject->m_Creator.GetChars();
  meta->date = mediaObject->m_Date.GetChars();

#if __cplusplus >= 201703L
  auto populateRoles = [](auto& target, const auto& source) {
    for (auto it = source.GetFirstItem(); it; ++it) {
      MetaDataPersonRole role;
      role.name = it->name.GetChars();
      role.role = it->role.GetChars();
      target.push_back(role);
    }
  };
#else
  // Lambda to populate person roles
  auto populateRoles = [](std::vector<MetaDataPersonRole>& target, const PLT_PersonRoles& source) {
    for (auto it = source.GetFirstItem(); it; ++it) {
      MetaDataPersonRole role;
      role.name = it->name.GetChars();
      role.role = it->role.GetChars();
      target.push_back(role);
    }
  };
#endif

  populateRoles(meta->people.artists, mediaObject->m_People.artists);
  populateRoles(meta->people.actors, mediaObject->m_People.actors);
  populateRoles(meta->people.authors, mediaObject->m_People.authors);

  meta->people.producer = mediaObject->m_People.producer.GetChars();
  meta->people.director = mediaObject->m_People.director.GetChars();
  meta->people.publisher = mediaObject->m_People.publisher.GetChars();
  meta->people.contributor = mediaObject->m_People.contributor.GetChars();

  // Affiliation info
  for (auto it = mediaObject->m_Affiliation.genres.GetFirstItem(); it; ++it) {
    if (!it->IsEmpty()) meta->affiliation.genres.push_back(it->GetChars());
  }
  meta->affiliation.album = mediaObject->m_Affiliation.album.GetChars();
  meta->affiliation.playlist = mediaObject->m_Affiliation.playlist.GetChars();

  // Description
  meta->description.description = mediaObject->m_Description.description.GetChars();
  meta->description.longDescription = mediaObject->m_Description.long_description.GetChars();
  meta->description.iconURI = mediaObject->m_Description.icon_uri.GetChars();
  meta->description.region = mediaObject->m_Description.region.GetChars();
  meta->description.rating = mediaObject->m_Description.rating.GetChars();
  meta->description.rights = mediaObject->m_Description.rights.GetChars();
  meta->description.date = mediaObject->m_Description.date.GetChars();
  meta->description.language = mediaObject->m_Description.language.GetChars();

  // Extra info
  for (auto it = mediaObject->m_ExtraInfo.album_arts.GetFirstItem(); it; ++it) {
    if (!it->uri.IsEmpty()) {
      MetaDataAlbumArtInfo albumArt;
      decodeURI = NPT_Uri::PercentDecode(it->uri.GetChars());
      albumArt.uri = decodeURI.GetChars();
      if (!it->dlna_profile.IsEmpty()) albumArt.dlnaProfile = it->dlna_profile.GetChars();
      meta->extraInfo.albumArts.push_back(albumArt);
    }
  }
  meta->extraInfo.artistDiscographyURI = mediaObject->m_ExtraInfo.artist_discography_uri.GetChars();
  meta->extraInfo.lyricsURI = mediaObject->m_ExtraInfo.lyrics_uri.GetChars();
  for (auto it = mediaObject->m_ExtraInfo.relations.GetFirstItem(); it; ++it) {
    if (!it->IsEmpty()) meta->extraInfo.relations.push_back(it->GetChars());
  }

  // Misc info
  meta->miscInfo.dvdregioncode = mediaObject->m_MiscInfo.dvdregioncode;
  meta->miscInfo.originalTrackNumber = mediaObject->m_MiscInfo.original_track_number;
  meta->miscInfo.toc = mediaObject->m_MiscInfo.toc.GetChars();
  meta->miscInfo.userAnnotation = mediaObject->m_MiscInfo.user_annotation.GetChars();

  // Resources
  for (NPT_Cardinal i = 0; i < mediaObject->m_Resources.GetItemCount(); ++i) {
    const auto& res = mediaObject->m_Resources[i];
    MetaDataResource resource;
    NPT_String tmpURI = NPT_Uri::PercentDecode(res.m_Uri.GetChars());
    resource.uri = tmpURI.GetChars();
    resource.protocolInfo = res.m_ProtocolInfo.ToString().GetChars();
    resource.duration = (res.m_Duration != (NPT_UInt32)-1) ? res.m_Duration : 0;
    resource.size = (res.m_Size != (NPT_UInt32)-1) ? res.m_Size : 0;
    resource.protection = res.m_Protection.GetChars();
    resource.bitrate = (res.m_Bitrate != (NPT_UInt32)-1) ? res.m_Bitrate : 0;
    resource.bitsPerSample = (res.m_BitsPerSample != (NPT_UInt32)-1) ? res.m_BitsPerSample : 0;
    resource.sampleFrequency = (res.m_SampleFrequency != (NPT_UInt32)-1) ? res.m_SampleFrequency : 0;
    resource.nbAudioChannels = (res.m_NbAudioChannels != (NPT_UInt32)-1) ? res.m_NbAudioChannels : 0;
    resource.resolution = res.m_Resolution.GetChars();
    resource.colorDepth = res.m_ColorDepth;
    meta->resources.push_back(resource);
  }

  return meta;
}

static inline void upnp_update_playlist_from_metadata(std::shared_ptr<MediaItem> mediaItem, std::shared_ptr<MetaData> metaData) {
  if (!mediaItem || !metaData) return;

  // Basic info
  mediaItem->origin = MediaItemOrigin::UPnP;
  mediaItem->type = metaData->type;

  // Resources
  if (!metaData->resources.empty()) {
    const auto& res = metaData->resources[0];
    mediaItem->uri = res.uri;
    mediaItem->duration = res.duration;
  }

  mediaItem->title = metaData->title;

  // Genre
  if (!metaData->affiliation.genres.empty()) {
    mediaItem->genre = metaData->affiliation.genres[0];
  }

  // Artist / AlbumArtist
  mediaItem->albumArtist.clear();
  mediaItem->artist.clear();

  for (const auto& artistRole : metaData->people.artists) {
    if (artistRole.role == "AlbumArtist" && !artistRole.name.empty()) {
      mediaItem->albumArtist = artistRole.name;
    } else if (artistRole.role == "Performer" && !artistRole.name.empty()) {
      mediaItem->artist = artistRole.name;
    }
  }

  // Fallbacks
  if (mediaItem->artist.empty() && !metaData->people.artists.empty()) {
    mediaItem->artist = metaData->people.artists[0].name;
  }
  if (mediaItem->artist.empty() && !metaData->creator.empty()) {
    mediaItem->artist = metaData->creator;
  }

  mediaItem->date = metaData->date;
  mediaItem->album = metaData->affiliation.album;
  mediaItem->trackNumber = metaData->miscInfo.originalTrackNumber;

  // Album art
  /* TODO: Find a way to find best album art URI here if multiple items */
  if (!metaData->extraInfo.albumArts.empty()) {
    mediaItem->albumArtURI = metaData->extraInfo.albumArts[0].uri;

    // Hack for full resolution cover art
    auto pos = mediaItem->albumArtURI.rfind("?scale=");
    if (pos != std::string::npos) {
      mediaItem->albumArtURI = mediaItem->albumArtURI.substr(0, pos + 7);
    }
  }

  // Attach metadata
  mediaItem->setMetaData(metaData);
}
