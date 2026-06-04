// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Yann Poirier

#pragma once
#include <atomic>
#include <cstddef>
#include <type_traits>
#include <vector>

namespace gf::dsp {

// Single-producer / single-consumer lock-free ring buffer.
//
// Exactly ONE thread may call push() and exactly ONE (other) thread may call
// pop(). This is the primitive for moving data across the audio-thread
// boundary (e.g. captured input frames -> recording thread) without locks or
// allocation. push()/pop() are wait-free and RT-safe.
template <typename T>
class SpscRingBuffer {
  static_assert(std::is_trivially_copyable<T>::value,
                "SpscRingBuffer requires a trivially copyable element type");

 public:
  SpscRingBuffer() = default;
  explicit SpscRingBuffer(size_t min_capacity) { reset(min_capacity); }

  // Allocates capacity (rounded up to a power of two) and clears the buffer.
  // NOT RT-safe: call before the audio stream starts.
  void reset(size_t min_capacity) {
    size_t cap = 1;
    while (cap < min_capacity) cap <<= 1;
    capacity_ = cap;
    mask_ = cap - 1;
    buffer_.assign(cap, T{});
    head_.store(0, std::memory_order_relaxed);
    tail_.store(0, std::memory_order_relaxed);
  }

  // Producer side. Writes up to `count` elements; returns how many were
  // written (fewer if the buffer is near full). RT-safe.
  size_t push(const T* data, size_t count) {
    const size_t head = head_.load(std::memory_order_relaxed);
    const size_t tail = tail_.load(std::memory_order_acquire);
    const size_t space = capacity_ - (head - tail);
    const size_t n = count < space ? count : space;
    for (size_t i = 0; i < n; ++i) buffer_[(head + i) & mask_] = data[i];
    head_.store(head + n, std::memory_order_release);
    return n;
  }

  // Consumer side. Reads up to `count` elements; returns how many were read.
  // RT-safe.
  size_t pop(T* data, size_t count) {
    const size_t tail = tail_.load(std::memory_order_relaxed);
    const size_t head = head_.load(std::memory_order_acquire);
    const size_t avail = head - tail;
    const size_t n = count < avail ? count : avail;
    for (size_t i = 0; i < n; ++i) data[i] = buffer_[(tail + i) & mask_];
    tail_.store(tail + n, std::memory_order_release);
    return n;
  }

  size_t available() const {
    return head_.load(std::memory_order_acquire) - tail_.load(std::memory_order_acquire);
  }
  size_t capacity() const { return capacity_; }

 private:
  size_t capacity_ = 0;
  size_t mask_ = 0;
  std::vector<T> buffer_;
  std::atomic<size_t> head_{0};  // advanced by producer
  std::atomic<size_t> tail_{0};  // advanced by consumer
};

}  // namespace gf::dsp
