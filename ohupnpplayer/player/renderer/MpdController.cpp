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

#include "MpdController.h"

#include "MyLogger.h"

#include <errno.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include <cmath>
#include <cstring>
#include <stdexcept>

/* ------------------------------------------------------------ */

/*
  Notes on improvements in this file:
  - use host_ / port_ when creating mpd connection (previously used nullptr/0)
  - check epoll_ctl and epoll_wait errors and handle EINTR/EAGAIN
  - safely read/write eventfd and timerfd (handle EINTR/EAGAIN)
  - remove MPD fd from epoll before freeing connection
  - avoid data races when invoking callbacks by copying them under mutex
*/

static ssize_t safe_write_fd(int fd, const void* buf, size_t count) {
  const char* p = static_cast<const char*>(buf);
  size_t left = count;
  while (left > 0) {
    ssize_t n = write(fd, p, left);
    if (n >= 0) {
      p += n;
      left -= static_cast<size_t>(n);
      continue;
    }
    if (errno == EINTR) continue;
    if (errno == EAGAIN) {
      // Non-blocking fd full — yield briefly and retry (small backoff)
      // it's rare but we should not silently ignore the wake-up
      ::usleep(1000);
      continue;
    }
    return -1;
  }
  return static_cast<ssize_t>(count);
}

static ssize_t safe_read_fd(int fd, void* buf, size_t count) {
  char* p = static_cast<char*>(buf);
  size_t left = count;
  while (left > 0) {
    ssize_t n = read(fd, p, left);
    if (n > 0) {
      p += n;
      left -= static_cast<size_t>(n);
      continue;
    }
    if (n == 0) return static_cast<ssize_t>(count - left); // EOF
    if (errno == EINTR) continue;
    if (errno == EAGAIN) return static_cast<ssize_t>(count - left); // no more data
    return -1;
  }
  return static_cast<ssize_t>(count);
}

MpdController::MpdController(std::string host, int port, std::chrono::milliseconds update_interval)
    : host_(std::move(host)), port_(port), update_interval_(update_interval) {
  epoll_fd_ = epoll_create1(0);
  if (epoll_fd_ < 0) throw std::runtime_error("epoll_create1 failed");

  wake_fd_ = eventfd(0, EFD_NONBLOCK);
  if (wake_fd_ < 0) {
    close(epoll_fd_);
    throw std::runtime_error("eventfd failed");
  }

  timer_fd_ = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
  if (timer_fd_ < 0) {
    close(epoll_fd_);
    close(wake_fd_);
    throw std::runtime_error("timerfd_create failed");
  }

  itimerspec ts{};
  ts.it_value.tv_sec = update_interval_.count() / 1000;
  ts.it_value.tv_nsec = (update_interval_.count() % 1000) * 1000000;
  ts.it_interval = ts.it_value;

  if (timerfd_settime(timer_fd_, 0, &ts, nullptr) < 0) {
    close(epoll_fd_);
    close(wake_fd_);
    close(timer_fd_);
    throw std::runtime_error("timerfd_settime failed");
  }

  auto add_fd = [&](int fd) {
    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) < 0) {
      int e = errno;
      close(epoll_fd_);
      close(wake_fd_);
      close(timer_fd_);
      throw std::runtime_error(std::string("epoll_ctl ADD failed: ") + std::strerror(e));
    }
  };

  add_fd(wake_fd_);
  add_fd(timer_fd_);

  thread_ = std::thread(&MpdController::thread_main, this);
}

/* ------------------------------------------------------------ */

MpdController::~MpdController() {
  running_ = false;

  uint64_t one = 1;
  (void)safe_write_fd(wake_fd_, &one, sizeof(one));

  if (thread_.joinable()) thread_.join();

  // Best effort remove known fds from epoll before closing
  if (epoll_fd_ >= 0) {
    if (wake_fd_ >= 0) epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, wake_fd_, nullptr);
    if (timer_fd_ >= 0) epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, timer_fd_, nullptr);
  }

  if (epoll_fd_ >= 0) close(epoll_fd_);
  if (wake_fd_ >= 0) close(wake_fd_);
  if (timer_fd_ >= 0) close(timer_fd_);
}

/* ------------------------------------------------------------ */

void MpdController::enqueue(std::function<void(mpd_connection*)> fn) {
  {
    std::lock_guard<std::mutex> lk(mutex_);
    queue_.push(std::move(fn));
  }
  uint64_t one = 1;
  (void)safe_write_fd(wake_fd_, &one, sizeof(one));
}

/* ------------------------------------------------------------ */
/* public API */

void MpdController::play() {
  enqueue([this](auto* c) {
    suppress_track_end_once_ = true;
    mpd_run_play(c);
  });
}

void MpdController::pause(bool p) {
  enqueue([p](auto* c) { mpd_run_pause(c, p); });
}

void MpdController::stop() {
  enqueue([this](auto* c) {
    suppress_track_end_once_ = true;
    mpd_run_stop(c);
  });
}

void MpdController::next() {
  enqueue([this](auto* c) {
    suppress_track_end_once_ = true;
    mpd_run_next(c);
  });
}
void MpdController::previous() {
  enqueue([this](auto* c) {
    suppress_track_end_once_ = true;
    mpd_run_previous(c);
  });
}

void MpdController::clear() {
  enqueue([](auto* c) { mpd_run_clear(c); });
}

void MpdController::add(const std::string& uri) {
  enqueue([uri](auto* c) { mpd_run_add(c, uri.c_str()); });
}

std::future<void> MpdController::seek(const float seconds, const bool relative) {
  auto p = std::make_shared<std::promise<void>>();
  enqueue([p, seconds, relative](mpd_connection* conn) {
    if (!mpd_run_seek_current(conn, seconds, relative))
      p->set_exception(std::make_exception_ptr(std::runtime_error("MPD seek failed")));
    else
      p->set_value();
  });
  return p->get_future();
}

/* ------------------------------------------------------------ */
/* futures */

std::future<int> MpdController::get_volume() {
  auto p = std::make_shared<std::promise<int>>();
  enqueue([p](mpd_connection* c) {
    auto* st = mpd_run_status(c);
    if (!st) {
      p->set_exception(std::make_exception_ptr(std::runtime_error("status failed")));
      return;
    }
    int v = mpd_status_get_volume(st);
    mpd_status_free(st);
    p->set_value(v);
  });
  return p->get_future();
}

std::future<void> MpdController::set_volume(int vol) {
  auto p = std::make_shared<std::promise<void>>();
  enqueue([p, vol](mpd_connection* c) {
    if (!mpd_run_set_volume(c, vol))
      p->set_exception(std::make_exception_ptr(std::runtime_error("set_volume failed")));
    else
      p->set_value();
  });
  return p->get_future();
}

#if 0
std::future<MpdController::Metadata> MpdController::get_metadata()
{
    std::promise<Metadata> p;
    auto fut = p.get_future();
    enqueue([p = std::move(p)](mpd_connection*) mutable {
        p.set_value(Metadata{});
    });
    return fut;
}
#else
std::future<MpdController::Metadata> MpdController::get_metadata() {
  auto p = std::make_shared<std::promise<Metadata>>();
  auto fut = p->get_future();
  enqueue([p](mpd_connection*) { p->set_value(Metadata{}); });
  return fut;
}
#endif
/* ------------------------------------------------------------ */

void MpdController::on_metadata(MetadataCallback cb) {
  std::lock_guard<std::mutex> lk(mutex_);
  metadata_cb_ = std::move(cb);
}

void MpdController::on_position(PositionCallback cb) {
  std::lock_guard<std::mutex> lk(mutex_);
  position_cb_ = std::move(cb);
}

void MpdController::on_track_ended(TrackEndedCallback cb) {
  std::lock_guard<std::mutex> lk(mutex_);
  track_ended_cb_ = std::move(cb);
}

/* ---------- helpers ---------- */

uint32_t MpdController::interpolated_position_ms() const {
  return position_.elapsed_ms;
}

/* ---------- MPD ---------- */

mpd_connection* MpdController::connect() {
  // Use configured host_ and port_ (previously used nullptr/0)
  mpd_connection* c = mpd_connection_new(host_.empty() ? nullptr : host_.c_str(), port_, 30000);
  if (!c || mpd_connection_get_error(c) != MPD_ERROR_SUCCESS) {
    if (c) mpd_connection_free(c);
    return nullptr;
  }

  const unsigned int* vers = mpd_connection_get_server_version(c);
  if (vers) ML_LOG_INFO("MPDCLi::openconn: mpd protocol version: %d.%d.%d\n", vers[0], vers[1], vers[2]);

  int fd = mpd_connection_get_fd(c);
  if (fd >= 0) {
    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) < 0) {
      ML_LOG_ERROR("epoll_ctl ADD mpd fd failed: %s\n", std::strerror(errno));
      mpd_connection_free(c);
      return nullptr;
    }
  }

  return c;
}

void MpdController::update_metadata(mpd_connection* c) {
  mpd_song* s = mpd_run_current_song(c);
  if (!s) return;

  Metadata m;
  if (auto v = mpd_song_get_tag(s, MPD_TAG_ARTIST, 0)) m.artist = v;
  if (auto v = mpd_song_get_tag(s, MPD_TAG_TITLE, 0)) m.title = v;
  if (auto v = mpd_song_get_tag(s, MPD_TAG_ALBUM, 0)) m.album = v;

  mpd_song_free(s);

  if (m.artist != metadata_.artist || m.title != metadata_.title || m.album != metadata_.album) {
    metadata_ = m;
    // copy callback under lock to avoid data race with on_metadata()
    MetadataCallback cb;
    {
      std::lock_guard<std::mutex> lk(mutex_);
      cb = metadata_cb_;
    }
    if (cb) cb(metadata_);
  }
}

void MpdController::update_position(mpd_connection* c) {
  mpd_status* st = mpd_run_status(c);
  if (!st) return;

  prev_position_ = position_;

  position_.state = mpd_status_get_state(st);
  position_.song_id = mpd_status_get_song_id(st);
  position_.elapsed_ms = mpd_status_get_elapsed_ms(st);
  position_.total_ms = mpd_status_get_total_time(st) * 1000;
  position_.gapless = mpd_status_get_crossfade(st) == 0;
  position_.next_song_prefetched = mpd_status_get_next_song_id(st) != -1;

  mpd_status_free(st);

  /* ----------------------------------------------------
   * Reset track-ended guard when playback (re)starts
   * ---------------------------------------------------- */
  if (position_.state == MPD_STATE_PLAY && prev_position_.state != MPD_STATE_PLAY) {
    last_ended_song_id_ = -1;
  }

  bool ended = false;

  if (suppress_track_end_once_) {
    suppress_track_end_once_ = false;
    goto emit_position_only;
  }

  /* Reset guard only on real playback start */
  if (position_.state == MPD_STATE_PLAY && prev_position_.state != MPD_STATE_PLAY && position_.song_id >= 0) {
    last_ended_song_id_ = -1;
  }

  /* -------- REAL TRACK END -------- */
  if (prev_position_.song_id >= 0 && prev_position_.state == MPD_STATE_PLAY && prev_position_.song_id != last_ended_song_id_) {
    /* Gapless or skip */
    if (position_.song_id >= 0 && position_.song_id != prev_position_.song_id) {
      ended = true;
      last_ended_song_id_ = prev_position_.song_id;
    }

    /* Natural end or explicit stop */
    else if (position_.state == MPD_STATE_STOP) {
      ended = true;
      last_ended_song_id_ = prev_position_.song_id;
    }
  }

  /* Streams (song_id == -1): NEVER emit track-ended */

  if (ended) {
    TrackEndedCallback tcb;
    {
      std::lock_guard<std::mutex> lk(mutex_);
      tcb = track_ended_cb_;
    }
    if (tcb) tcb();
  }

emit_position_only: {
  // Emit position only if something meaningful changed.
#if 1
  auto secs = [](int64_t ms) { return ms / 1000; };

  bool elapsed_seconds_changed = secs(position_.elapsed_ms) != secs(prev_position_.elapsed_ms);

  bool position_changed = position_.state != prev_position_.state || position_.song_id != prev_position_.song_id || elapsed_seconds_changed ||
                          position_.total_ms != prev_position_.total_ms || position_.gapless != prev_position_.gapless ||
                          position_.next_song_prefetched != prev_position_.next_song_prefetched;
#else
  bool position_changed = position_.state != prev_position_.state || position_.song_id != prev_position_.song_id ||
                          position_.elapsed_ms != prev_position_.elapsed_ms || position_.total_ms != prev_position_.total_ms ||
                          position_.gapless != prev_position_.gapless || position_.next_song_prefetched != prev_position_.next_song_prefetched;
#endif

  if (position_changed) {
    PositionCallback pcb;
    {
      std::lock_guard<std::mutex> lk(mutex_);
      pcb = position_cb_;
    }
    if (pcb) pcb(position_);
  }
}
}

/* ---------- thread ---------- */
void MpdController::thread_main() {
  mpd_connection* conn = nullptr;
  epoll_event evs[8];

  while (running_) {
    if (!conn) conn = connect();

    int n = epoll_wait(epoll_fd_, evs, static_cast<int>(std::size(evs)), -1);
    if (n < 0) {
      if (errno == EINTR) continue;
      ML_LOG_ERROR("epoll_wait failed: %s\n", std::strerror(errno));
      break;
    }

    for (int i = 0; i < n; ++i) {
      int fd = evs[i].data.fd;

      if (fd == wake_fd_) {
        uint64_t tmp = 0;
        (void)safe_read_fd(wake_fd_, &tmp, sizeof(tmp));

        std::queue<std::function<void(mpd_connection*)>> q;
        {
          std::lock_guard<std::mutex> lk(mutex_);
          std::swap(q, queue_);
        }
        while (!q.empty() && conn) {
          try {
            q.front()(conn);
          } catch (const std::exception& e) {
            ML_LOG_ERROR("Exception in queued MPD job: %s\n", e.what());
          } catch (...) {
            ML_LOG_ERROR("Unknown exception in queued MPD job\n");
          }
          q.pop();
        }
      } else if (fd == timer_fd_) {
        uint64_t tmp = 0;
        (void)safe_read_fd(timer_fd_, &tmp, sizeof(tmp));
        if (conn) update_position(conn);
      } else if (conn && fd == mpd_connection_get_fd(conn)) {
        // Wait for MPD events (blocks until an event arrives)
        if (!mpd_run_idle_mask(conn, static_cast<mpd_idle>(MPD_IDLE_PLAYER | MPD_IDLE_QUEUE))) {
          // If idle failed, we'll detect the error later and reconnect
        } else {
          update_metadata(conn);
          update_position(conn);
        }
      } else {
        // Unknown fd — ignore
      }
    }

    if (conn && mpd_connection_get_error(conn) != MPD_ERROR_SUCCESS) {
      int mfd = mpd_connection_get_fd(conn);
      if (mfd >= 0) epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, mfd, nullptr);
      mpd_connection_free(conn);
      conn = nullptr;
    }
  }

  if (conn) {
    int mfd = mpd_connection_get_fd(conn);
    if (mfd >= 0) epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, mfd, nullptr);
    mpd_connection_free(conn);
  }
}
