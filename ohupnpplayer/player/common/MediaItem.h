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

#include <MetaData.h>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

enum class MediaItemState {
  Normal,
  RetrievingMetaData,
  Delete
};

enum class MediaItemOrigin {
  Unknown,
  UPnP,
  OpenHome,
  MusicCatalogDynamicFS
};

class MediaItem {
 public:
  MediaItem() = default;
  ~MediaItem() = default;

  void setMetaData(std::shared_ptr<MetaData> metadata) {
    m_metaData = std::move(metadata);
  }

  std::shared_ptr<MetaData> getMetaData() const {
    return m_metaData;
  }

  bool operator==(const MediaItem& other) const {
    return m_metaData == other.m_metaData && uri == other.uri;
  }

  bool operator!=(const MediaItem& other) const {
    return !(*this == other);
  }

  static void dump(const std::shared_ptr<MediaItem>& item) {
    if (!item) {
      printf("media item dump: null\n");
      return;
    }

    printf("media item dump\n");

    // Map type to string
    const char* typeStr = nullptr;
    switch (item->type) {
      case kMetaDataTypeUnknown:
        typeStr = "unknown";
        break;
      case kMetaDataTypeContainer:
        typeStr = "container/folder";
        break;
      case kMetaDataTypeAudio:
        typeStr = "audio";
        break;
      case kMetaDataTypeImage:
        typeStr = "image";
        break;
      case kMetaDataTypeVideo:
        typeStr = "video";
        break;
      case kMetaDataTypePlaylist:
        typeStr = "playlist";
        break;
      default:
        typeStr = "invalid";
        break;
    }
    printf(" type [%s]\n", typeStr);

    // Helper lambdas
    auto printStrIfNotEmpty = [](const char* name, const std::string& val) {
      if (!val.empty()) printf("  %s [%s]\n", name, val.c_str());
    };

    auto printUIntIfNonZero = [](const char* name, uint32_t val) {
      if (val != 0) printf("  %s [%u]\n", name, val);
    };

    // Print all fields
    printStrIfNotEmpty("UUID", item->UUID);
    printStrIfNotEmpty("uri", item->uri);
    printStrIfNotEmpty("title", item->title);
    printStrIfNotEmpty("artist", item->artist);
    printStrIfNotEmpty("albumArtist", item->albumArtist);
    printStrIfNotEmpty("date", item->date);
    printStrIfNotEmpty("album", item->album);
    printUIntIfNonZero("duration", item->duration);
    printUIntIfNonZero("trackNumber", item->trackNumber);
    printStrIfNotEmpty("albumArtURI", item->albumArtURI);
    printStrIfNotEmpty("genre", item->genre);
    printStrIfNotEmpty("orientation", item->orientation);
    printStrIfNotEmpty("resolution", item->resolution);

    if (item->ohPltID != static_cast<uint32_t>(-1)) {
      printf("  ohPltID [%u]\n", item->ohPltID);
      printStrIfNotEmpty("ohPltMetadata", item->ohPltMetadata);
    }

    MetaData::dump(item->getMetaData());
  }

 public:
  MetaDataType type{kMetaDataTypeUnknown};
  std::string UUID;
  std::string uri;
  std::string title;
  std::string artist;
  std::string albumArtist;
  std::string date;
  std::string album;
  uint32_t duration{0};
  uint32_t trackNumber{0};
  std::string albumArtURI;
  std::string genre;
  std::string orientation;
  std::string resolution;
  MediaItemState state{MediaItemState::Normal};
  MediaItemOrigin origin{MediaItemOrigin::Unknown};
  uint32_t ohPltID{static_cast<uint32_t>(-1)};
  std::string ohPltMetadata;

 private:
  std::shared_ptr<MetaData> m_metaData;
};

// STL definitions
using MediaItems = std::vector<std::shared_ptr<MediaItem>>;
using MediaItemsIt = MediaItems::iterator;
using MediaItemsConstIt = MediaItems::const_iterator;
