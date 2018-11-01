// Copyright (c) 2015-2016, ETH Zurich, Wyss Zurich, Zurich Eye
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of the ETH Zurich, Wyss Zurich, Zurich Eye nor the
//       names of its contributors may be used to endorse or promote products
//       derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL ETH Zurich, Wyss Zurich, Zurich Eye BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#pragma once

#include <map>
#include <tuple>
#include <thread>
#include <utility>
#include <mutex>

#include <Eigen/Core>

#include <glog/logging.h>


namespace common {

    // Oldest entry: buffer.begin(), newest entry: buffer.rbegin()
    template <typename Scalar, int Dim>
    class Buffer {
    public:
        using Vector = Eigen::Matrix<Scalar, Dim, 1>;
        // https://github.com/JakobEngel/dso/issues/111
        using VectorBuffer = std::map<int64_t,
        Vector, std::less<int64_t>,
                Eigen::aligned_allocator<std::pair<const int64_t, Vector>>>;


        static constexpr int kDim = Dim;

        Buffer() = default;
        Buffer(double buffer_size_seconds)
                : buffer_size_nanosec_(secToNanosec(buffer_size_seconds))
        {}

        Buffer(const Buffer& from)
        {
            std::lock_guard<std::mutex>(from.mutex_);
            if (from.buffer_.size() != 0)
            {
                throw std::runtime_error("Non-empty buffers are not copyable.");
            }
            buffer_size_nanosec_ = from.buffer_size_nanosec_;
        }

        inline void insert(int64_t stamp, const Vector& data)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            buffer_[stamp] = data;
            if(buffer_size_nanosec_ > 0)
            {
                removeDataBeforeTimestamp_impl(
                        buffer_.rbegin()->first - buffer_size_nanosec_);

            }
        }

        //! Get value with timestamp closest to stamp. Boolean in returns if successful.
        std::tuple<int64_t, Vector, bool> getNearestValue(int64_t stamp);

        //! Get oldest value in buffer.
        std::tuple<int64_t, Eigen::Matrix<Scalar, Dim, 1>, bool> getOldestValue() const;

        //! Get newest value in buffer.
        std::tuple<int64_t, Eigen::Matrix<Scalar, Dim, 1>, bool> getNewestValue() const;

        //! Get timestamps of newest and oldest entry.
        std::tuple<int64_t, int64_t, bool> getOldestAndNewestStamp() const;

        /*! @brief Get Values between timestamps.
         *
         * If timestamps are not matched, the values
         * are interpolated. Returns a vector of timestamps and a block matrix with
         * values as columns. Returns empty matrices if not successful.
         */
        std::pair<Eigen::Matrix<int64_t, Eigen::Dynamic, 1>, Eigen::Matrix<Scalar, Dim, Eigen::Dynamic> >
        getBetweenValuesInterpolated(int64_t stamp_from, int64_t stamp_to);


        std::tuple<int64_t,Eigen::Matrix<Scalar, Dim, 1>, bool>
        getValueAtInterpolateIfNeeded(int64_t stamp);


        std::tuple<int64_t,Eigen::Matrix<Scalar, Dim, 1>, bool> getValueAt(int64_t stamp) const;

        inline void clear()
        {
            std::lock_guard<std::mutex> lock(mutex_);
            buffer_.clear();
        }

        inline size_t size() const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return buffer_.size();
        }

        inline bool empty() const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return buffer_.empty();
        }

        inline void removeDataBeforeTimestamp(int64_t stamp)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            removeDataBeforeTimestamp_impl(stamp);
        }

        inline void removeDataOlderThan(double seconds)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if(buffer_.empty())
                return;

            removeDataBeforeTimestamp_impl(
                    buffer_.rbegin()->first - secToNanosec(seconds));
        }

        inline void lock() const
        {
            mutex_.lock();
        }

        inline void unlock() const
        {
            mutex_.unlock();
        }

        const VectorBuffer& data() const
        {

            return buffer_;
        }

        typename VectorBuffer::iterator iterator_equal_or_before(int64_t stamp);

        typename VectorBuffer::iterator iterator_equal_or_after(int64_t stamp);

    protected:
        mutable std::mutex mutex_;
        VectorBuffer buffer_;
        int64_t buffer_size_nanosec_ = -1; // Negative means, no fixed size.

        inline constexpr int64_t secToNanosec(double seconds) {
            return static_cast<int64_t>(seconds * 1e9);
        }

        inline void removeDataBeforeTimestamp_impl(int64_t stamp)
        {
            auto it = buffer_.lower_bound(stamp);
            buffer_.erase(buffer_.begin(), it);
        }
    };

} // namespace common

#include "buffer-inl.h"