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

#include <mpd/client.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

class MpdController {
 public:
  /* ---------- snapshots ---------- */

  struct Metadata {
    std::string artist;
    std::string title;
    std::string album;
  };

  struct PositionSnapshot {
    mpd_state state = MPD_STATE_STOP;
    int song_id = -1;
    uint32_t elapsed_ms = 0;
    uint32_t total_ms = 0;
    bool gapless = false;
    bool next_song_prefetched = false;
  };

  /* ---------- callbacks ---------- */
  using MetadataCallback = std::function<void(const Metadata&)>;
  using PositionCallback = std::function<void(const PositionSnapshot&)>;
  using TrackEndedCallback = std::function<void()>;

  explicit MpdController(std::string host = "localhost", int port = 6600, std::chrono::milliseconds update_interval = std::chrono::milliseconds(100));

  ~MpdController();

  /* fire-and-forget commands */
  void play();
  void pause(bool p);
  void stop();
  void next();
  void previous();

  void clear();
  void add(const std::string& uri);
  std::future<void> seek(const float seconds, const bool absolute);

  /* request/response */
  std::future<int> get_volume();
  std::future<void> set_volume(int vol);
  std::future<Metadata> get_metadata();

  /* ---------- callbacks ---------- */

  void on_metadata(MetadataCallback cb);
  void on_position(PositionCallback cb);
  void on_track_ended(TrackEndedCallback cb);

  /* ---------- position ---------- */

  uint32_t interpolated_position_ms() const;

 private:
  void thread_main();
  mpd_connection* connect();

  void enqueue(std::function<void(mpd_connection*)> fn);

  void update_metadata(mpd_connection*);
  void update_position(mpd_connection*);

 private:
  std::string host_;
  int port_;
  std::chrono::milliseconds update_interval_;

  std::thread thread_;
  std::atomic<bool> running_{true};

  int epoll_fd_ = -1;
  int timer_fd_ = -1;
  int wake_fd_ = -1;

  mutable std::mutex mutex_;
  std::queue<std::function<void(mpd_connection*)>> queue_;

  Metadata metadata_;
  PositionSnapshot position_;
  PositionSnapshot prev_position_;

  int last_ended_song_id_ = -1;
  bool suppress_track_end_once_ = false;

  std::function<void(const Metadata&)> metadata_cb_;
  std::function<void(const PositionSnapshot&)> position_cb_;
  std::function<void()> track_ended_cb_;
};
