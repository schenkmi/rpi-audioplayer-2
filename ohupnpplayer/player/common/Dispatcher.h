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

#if __cplusplus >= 201703L
// C++17 implementation
#include <atomic>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <typeindex>
#include <unordered_map>
#include <variant>

// ---------------------- Message Types ----------------------
struct UpdatePlayTimeMessage {
  uint32_t time;
  uint32_t duration;
};
struct StopMessage {}; // for thread exit
struct PlayNextMessage {};
struct UpdateStateMessage {};

// Variant to hold all possible message types
using Message = std::variant<UpdatePlayTimeMessage, StopMessage, PlayNextMessage, UpdateStateMessage>;

// ---------------------- Thread-safe Queue ----------------------
class MessageQueue {
 public:
  void push(Message msg) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      queue_.push(std::move(msg));
    }
    cv_.notify_one();
  }

  Message pop() {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [&] { return !queue_.empty(); });
    Message msg = std::move(queue_.front());
    queue_.pop();
    return msg;
  }

 private:
  std::queue<Message> queue_;
  std::mutex mutex_;
  std::condition_variable cv_;
};

// ---------------------- Dispatcher ----------------------
class Dispatcher {
 public:
  Dispatcher() : thread_(&Dispatcher::asyncService, this) {}

  ~Dispatcher() {
    send(StopMessage{}); // Clean shutdown signal
    if (thread_.joinable()) thread_.join();
  }

  void send(Message msg) {
    queue_.push(std::move(msg));
  }

  template <typename MsgType>
  void registerHandler(std::function<void(const MsgType&)> handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    handlers_[typeid(MsgType)] = [h = std::move(handler)](const Message& msg) { h(std::get<MsgType>(msg)); };
  }

 private:
  void asyncService() {
    for (;;) {
      Message msg = queue_.pop();

      // StopMessage always terminates the thread
      if (std::holds_alternative<StopMessage>(msg)) break;

      // Dispatch safely using std::visit
      std::visit(
          [this, &msg](auto&& m) {
            using T = std::decay_t<decltype(m)>;

            std::function<void(const Message&)> handler;
            {
              std::lock_guard<std::mutex> lock(mutex_);
              auto it = handlers_.find(typeid(T));
              if (it != handlers_.end()) handler = it->second;
            }

            if (handler) handler(msg);
          },
          msg);
    }
  }

  MessageQueue queue_;
  std::thread thread_;
  std::unordered_map<std::type_index, std::function<void(const Message&)> > handlers_;
  std::mutex mutex_;
};

class ThreadPoolDispatcher {
 public:
  explicit ThreadPoolDispatcher(size_t workers = 1) {
    for (size_t i = 0; i < workers; ++i) threads_.emplace_back(&ThreadPoolDispatcher::asyncService, this);
  }

  ~ThreadPoolDispatcher() {
    for (size_t i = 0; i < threads_.size(); ++i) queue_.push(StopMessage{});
    for (auto& t : threads_) t.join();
  }

  void send(Message msg) {
    queue_.push(std::move(msg));
  }

  template <typename MsgType>
  void registerHandler(std::function<void(const MsgType&)> handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    handlers_[typeid(MsgType)] = [h = std::move(handler)](const Message& msg) { h(std::get<MsgType>(msg)); };
  }

 private:
  void asyncService() {
    for (;;) {
      Message msg = queue_.pop();

      // StopMessage always terminates the thread
      if (std::holds_alternative<StopMessage>(msg)) break;

      // Dispatch safely using std::visit
      std::visit(
          [this, &msg](auto&& m) {
            using T = std::decay_t<decltype(m)>;

            std::function<void(const Message&)> handler;
            {
              std::lock_guard<std::mutex> lock(mutex_);
              auto it = handlers_.find(typeid(T));
              if (it != handlers_.end()) handler = it->second;
            }

            if (handler) handler(msg);
          },
          msg);
    }
  }

  MessageQueue queue_;
  std::vector<std::thread> threads_;
  std::unordered_map<std::type_index, std::function<void(const Message&)> > handlers_;
  std::mutex mutex_;
};

#else
// C++11 implementation
#include <atomic>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <typeindex>
#include <unordered_map>
#include <vector>

// ---------------------- Message Types ----------------------
struct BaseMessage {
  virtual ~BaseMessage() {}
};

struct UpdatePlayTimeMessage : BaseMessage {
  uint32_t time;
  uint32_t duration;
  UpdatePlayTimeMessage(uint32_t t, uint32_t d) : time(t), duration(d) {}
};

struct StopMessage : BaseMessage {};
struct PlayNextMessage : BaseMessage {};
struct UpdateStateMessage : BaseMessage {};

using MessagePtr = std::shared_ptr<BaseMessage>;

// ---------------------- Thread-safe Queue ----------------------
class MessageQueue {
 public:
  void push(MessagePtr msg) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      queue_.push(std::move(msg));
    }
    cv_.notify_one();
  }

  MessagePtr pop() {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [&] { return !queue_.empty(); });
    MessagePtr msg = std::move(queue_.front());
    queue_.pop();
    return msg;
  }

 private:
  std::queue<MessagePtr> queue_;
  std::mutex mutex_;
  std::condition_variable cv_;
};

// ---------------------- Dispatcher ----------------------
class Dispatcher {
 public:
  Dispatcher() : thread_(&Dispatcher::asyncService, this) {}

  ~Dispatcher() {
    send(std::make_shared<StopMessage>());
    if (thread_.joinable()) thread_.join();
  }

  void send(MessagePtr msg) {
    queue_.push(std::move(msg));
  }

  template <typename MsgType>
  void registerHandler(std::function<void(const MsgType&)> handler) {
    std::lock_guard<std::mutex> lock(mutex_);

    // C++11-compatible lambda capture: capture by value, then move inside
    auto h = std::move(handler); // move into local variable
    handlers_[typeid(MsgType)] = [h](MessagePtr msg) { h(*static_cast<MsgType*>(msg.get())); };
  }

 private:
  void asyncService() {
    for (;;) {
      MessagePtr msg = queue_.pop();

      if (dynamic_cast<StopMessage*>(msg.get())) break;

      std::function<void(MessagePtr)> handler;
      {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = handlers_.find(typeid(*msg));
        if (it != handlers_.end()) handler = it->second;
      }

      if (handler) handler(msg);
    }
  }

  MessageQueue queue_;
  std::thread thread_;
  std::unordered_map<std::type_index, std::function<void(MessagePtr)> > handlers_;
  std::mutex mutex_;
};

// ---------------------- ThreadPoolDispatcher ----------------------
class ThreadPoolDispatcher {
 public:
  explicit ThreadPoolDispatcher(size_t workers = 1) {
    for (size_t i = 0; i < workers; ++i) threads_.emplace_back(&ThreadPoolDispatcher::asyncService, this);
  }

  ~ThreadPoolDispatcher() {
    for (size_t i = 0; i < threads_.size(); ++i) queue_.push(std::make_shared<StopMessage>());
    for (auto& t : threads_) t.join();
  }

  void send(MessagePtr msg) {
    queue_.push(std::move(msg));
  }

  template <typename MsgType>
  void registerHandler(std::function<void(const MsgType&)> handler) {
    std::lock_guard<std::mutex> lock(mutex_);

    // C++11-compatible lambda capture: capture by value, then move inside
    auto h = std::move(handler); // move into local variable
    handlers_[typeid(MsgType)] = [h](MessagePtr msg) { h(*static_cast<MsgType*>(msg.get())); };
  }

 private:
  void asyncService() {
    for (;;) {
      MessagePtr msg = queue_.pop();

      if (dynamic_cast<StopMessage*>(msg.get())) break;

      std::function<void(MessagePtr)> handler;
      {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = handlers_.find(typeid(*msg));
        if (it != handlers_.end()) handler = it->second;
      }

      if (handler) handler(msg);
    }
  }

  MessageQueue queue_;
  std::vector<std::thread> threads_;
  std::unordered_map<std::type_index, std::function<void(MessagePtr)> > handlers_;
  std::mutex mutex_;
};
#endif
