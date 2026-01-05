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

#include <stdint.h>
#include <memory>
#include <string>
#include <vector>

/* meta data type (type of item) */
typedef enum {
  kMetaDataTypeUnknown,   /* unknown item					*/
  kMetaDataTypeContainer, /* container/folder item		*/
  kMetaDataTypeAudio,     /* audio item					*/
  kMetaDataTypeImage,     /* image item					*/
  kMetaDataTypeVideo,     /* video item					*/
  kMetaDataTypePlaylist,  /* playlist (.m3u, .pls)		*/
} MetaDataType;

/**
 * single meta data resource definition.
 */
class MetaDataResource {
 public:
  MetaDataResource()
      : uri(""),
        protocolInfo(""),
        duration(0),
        size(0),
        protection(""),
        bitrate(0),
        bitsPerSample(0),
        sampleFrequency(0),
        nbAudioChannels(0),
        resolution(""),
        colorDepth(0) {}

  virtual ~MetaDataResource() {}

 public:
  std::string uri;
  std::string protocolInfo;
  uint32_t duration; /* seconds */
  uint64_t size;
  std::string protection;
  uint32_t bitrate; /* bytes/seconds */
  uint32_t bitsPerSample;
  uint32_t sampleFrequency;
  uint32_t nbAudioChannels;
  std::string resolution;
  uint32_t colorDepth;
};

typedef std::vector<MetaDataResource> MetaDataResources;
typedef MetaDataResources::iterator MetaDataResourcesIt;
typedef MetaDataResources::const_iterator MetaDataResourcesConstIt;

/**
 * meta data exif definition.
 */
class MetaDataExif {
 public:
  MetaDataExif() : dateTime(""), dimensionX(""), dimensionY(""), orientation("") {}

  virtual ~MetaDataExif() {}

 public:
  std::string dateTime;   /* %d:%d:%d %d:%d:%d (year:month:day hours:minutes:seconds */
  std::string dimensionX; /* XxY (1920x1080)		*/
  std::string dimensionY;
  std::string orientation;
};

/**
 * single meta data person role.
 */
class MetaDataPersonRole {
 public:
  MetaDataPersonRole() : name(""), role("") {}

  virtual ~MetaDataPersonRole() {}

 public:
  std::string name;
  std::string role;
};

typedef std::vector<MetaDataPersonRole> MetaDataPersonRoles;
typedef MetaDataPersonRoles::iterator MetaDataPersonRolesIt;
typedef MetaDataPersonRoles::const_iterator MetaDataPersonRolesConstIt;

/**
 * meta data people info.
 */
class MetaDataPeopleInfo {
 public:
  MetaDataPeopleInfo() : producer(""), director(""), publisher(""), contributor("") {}

  virtual ~MetaDataPeopleInfo() {}

 public:
  MetaDataPersonRoles artists;
  MetaDataPersonRoles actors;
  MetaDataPersonRoles authors;
  std::string producer;    // TODO: can be multiple
  std::string director;    // TODO: can be multiple
  std::string publisher;   // TODO: can be multiple
  std::string contributor; // should match m_Creator (dc:creator) //TODO: can be multiple
};

typedef std::vector<std::string> MetaDataGenres;
typedef MetaDataGenres::iterator MetaDataGenresIt;
typedef MetaDataGenres::const_iterator MetaDataGenresConstIt;

/**
 * meta data affiliation info.
 */
class MetaDataAffiliationInfo {
 public:
  MetaDataAffiliationInfo() : album(""), playlist("") {}

  virtual ~MetaDataAffiliationInfo() {}

 public:
  MetaDataGenres genres;
  std::string album;    // TODO: can be multiple
  std::string playlist; // dc:title of the playlist item the content belongs too //TODO: can be multiple
};

/**
 * meta data description definition.
 */
class MetaDataDescription {
 public:
  MetaDataDescription() : description(""), longDescription(""), iconURI(""), region(""), rating(""), rights(""), date(""), language("") {}

  virtual ~MetaDataDescription() {}

 public:
  std::string description;
  std::string longDescription;
  std::string iconURI;
  std::string region;
  std::string rating;
  std::string rights; // TODO: can be multiple
  std::string date;
  std::string language;
};

/**
 * meta data album art definition.
 */
class MetaDataAlbumArtInfo {
 public:
  MetaDataAlbumArtInfo() : uri(""), dlnaProfile("") {}

  virtual ~MetaDataAlbumArtInfo() {}

 public:
  std::string uri;
  std::string dlnaProfile;
};

typedef std::vector<MetaDataAlbumArtInfo> MetaDataAlbumArtInfos;
typedef MetaDataAlbumArtInfos::iterator MetaDataAlbumArtInfosIt;
typedef MetaDataAlbumArtInfos::const_iterator MetaDataAlbumArtInfosConstIt;

typedef std::vector<std::string> MetaDataRelations;
typedef MetaDataRelations::iterator MetaDataRelationsIt;
typedef MetaDataRelations::const_iterator MetaDataRelationsConstIt;

/**
 * meta data extra info definition.
 */
class MetaDataExtraInfo {
 public:
  MetaDataExtraInfo() : artistDiscographyURI(""), lyricsURI("") {}

  virtual ~MetaDataExtraInfo() {}

 public:
  MetaDataAlbumArtInfos albumArts;
  std::string artistDiscographyURI; // artist_discography_uri;
  std::string lyricsURI;
  MetaDataRelations relations; // dc:relation
};

/**
 * meta data misc info definition.
 */
class MetaDataMiscInfo {
 public:
  MetaDataMiscInfo() : dvdregioncode(0), originalTrackNumber(0), toc(""), userAnnotation("") {}

  virtual ~MetaDataMiscInfo() {}

 public:
  uint32_t dvdregioncode;
  uint32_t originalTrackNumber;
  std::string toc;
  std::string userAnnotation; // TODO: can be multiple
};

class MetaData {
 public:
  MetaData() : type(kMetaDataTypeUnknown) {}
  virtual ~MetaData() = default;

  bool operator==(const MetaData& other) const {
    return type == other.type; // expand if needed
  }

  bool operator!=(const MetaData& other) const {
    return !(*this == other);
  }

  static void dump(const std::shared_ptr<MetaData>& data) {
    if (!data) return;

    printf("meta data dump\n");

    auto printStr = [](const char* name, const std::string& val) {
      if (!val.empty()) printf("  %s [%s]\n", name, val.c_str());
    };
    auto printUInt = [](const char* name, uint32_t val) {
      if (val != 0) printf("  %s [%u]\n", name, val);
    };
    auto printUInt64 = [](const char* name, uint64_t val) {
      if (val != 0) printf("  %s [%llu]\n", name, val);
    };

    // Type
    const char* typeStr = "unknown";
    switch (data->type) {
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
        break;
    }
    printf(" type [%s]\n", typeStr);

    printStr("title", data->title);
    printStr("creator", data->creator);
    printStr("date", data->date);

    // People info
    auto printPersonRoles = [&](const char* label, const std::vector<MetaDataPersonRole>& roles) {
      if (!roles.empty()) {
        printf("  %s count [%zu]\n", label, roles.size());
        size_t idx = 0;
        for (const auto& r : roles) printf("   [%zu] role [%s] name [%s]\n", idx++, r.role.c_str(), r.name.c_str());
      }
    };

    printPersonRoles("artists", data->people.artists);
    printPersonRoles("actors", data->people.actors);
    printPersonRoles("authors", data->people.authors);

    printStr("producer", data->people.producer);
    printStr("director", data->people.director);
    printStr("publisher", data->people.publisher);
    printStr("contributor", data->people.contributor);

    // Affiliation info
    if (!data->affiliation.genres.empty()) {
      printf("  genres count [%zu]\n", data->affiliation.genres.size());
      for (size_t i = 0; i < data->affiliation.genres.size(); ++i) printf("   [%zu] genre [%s]\n", i, data->affiliation.genres[i].c_str());
    }
    printStr("album", data->affiliation.album);
    printStr("playlist", data->affiliation.playlist);

    // Description
    printStr("description", data->description.description);
    printStr("longDescription", data->description.longDescription);
    printStr("iconURI", data->description.iconURI);
    printStr("region", data->description.region);
    printStr("rating", data->description.rating);
    printStr("rights", data->description.rights);
    printStr("language", data->description.language);
    printStr("date", data->description.date);

    // Extra info
    if (!data->extraInfo.albumArts.empty()) {
      printf("  album arts count [%zu]\n", data->extraInfo.albumArts.size());
      for (size_t i = 0; i < data->extraInfo.albumArts.size(); ++i) {
        const auto& art = data->extraInfo.albumArts[i];
        printStr("   URI", art.uri);
        printStr("   DLNA profile", art.dlnaProfile);
      }
    }
    printStr("artistDiscographyURI", data->extraInfo.artistDiscographyURI);
    printStr("lyricsURI", data->extraInfo.lyricsURI);
    if (!data->extraInfo.relations.empty()) {
      printf("  relations count [%zu]\n", data->extraInfo.relations.size());
      for (size_t i = 0; i < data->extraInfo.relations.size(); ++i) printf("   [%zu] relation [%s]\n", i, data->extraInfo.relations[i].c_str());
    }

    // Misc
    printUInt("DVD region code", data->miscInfo.dvdregioncode);
    printUInt("original track number", data->miscInfo.originalTrackNumber);
    printStr("toc", data->miscInfo.toc);
    printStr("user annotation", data->miscInfo.userAnnotation);

    // Resources
    if (!data->resources.empty()) {
      printf("  resources count [%zu]\n", data->resources.size());
      for (size_t i = 0; i < data->resources.size(); ++i) {
        const auto& res = data->resources[i];
        printf("   resource [%zu]\n", i);
        printStr("    URI", res.uri);
        printStr("    protocolInfo", res.protocolInfo);
        printUInt("    duration", res.duration);
        printUInt64("    size", res.size);
        printStr("    protection", res.protection);
        printUInt("    bitrate", res.bitrate);
        printUInt("    bitsPerSample", res.bitsPerSample);
        printUInt("    sampleFrequency", res.sampleFrequency);
        printUInt("    nbAudioChannels", res.nbAudioChannels);
        printStr("    resolution", res.resolution);
        printUInt("    colorDepth", res.colorDepth);
      }
    }

    // EXIF
    printStr("dateTime", data->exif.dateTime);
    printStr("dimensionX", data->exif.dimensionX);
    printStr("dimensionY", data->exif.dimensionY);
    printStr("orientation", data->exif.orientation);
  }

 public:
  MetaDataType type;
  std::string title;
  std::string creator;
  std::string date;

  MetaDataPeopleInfo people;
  MetaDataAffiliationInfo affiliation;
  MetaDataDescription description;
  MetaDataExtraInfo extraInfo;
  MetaDataMiscInfo miscInfo;
  MetaDataResources resources;
  MetaDataExif exif;
};
