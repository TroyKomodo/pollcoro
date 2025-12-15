module;

#include <cstddef>
#include <iterator>
#include <optional>
#include <utility>

export module pollcoro:sync_iter;

import :stream_awaitable;
import :waker;

export namespace pollcoro {
template<stream_awaitable StreamAwaitable>
class sync_iter_stream_iterator {
    using value_type = stream_awaitable_result_t<StreamAwaitable>;

    StreamAwaitable stream_;
    std::optional<value_type> current_value_;
    bool at_end_ = false;

    void advance() {
        if (at_end_)
            return;

        // Poll until we get a result
        while (true) {
            auto result = stream_.poll_next(waker());

            if (result.is_done()) {
                at_end_ = true;
                return;
            } else if (result.is_ready()) {
                current_value_ = result.take_result();
                return;
            }
        }
    }

  public:
    // Iterator type for range-based for loop
    class iterator {
        sync_iter_stream_iterator* parent_;

      public:
        using iterator_category = std::input_iterator_tag;
        using value_type = sync_iter_stream_iterator::value_type;
        using difference_type = std::ptrdiff_t;
        using pointer = value_type*;
        using reference = value_type&;

        iterator() : parent_(nullptr) {}

        explicit iterator(sync_iter_stream_iterator* parent) : parent_(parent) {}

        reference operator*() {
            return *parent_->current_value_;
        }

        pointer operator->() {
            return &*parent_->current_value_;
        }

        iterator& operator++() {
            parent_->advance();
            return *this;
        }

        iterator operator++(int) {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        bool operator==(const iterator& other) const {
            // End iterator has null parent or parent is at end
            bool this_at_end = (parent_ == nullptr) || parent_->at_end_;
            bool other_at_end = (other.parent_ == nullptr) || other.parent_->at_end_;
            return this_at_end == other_at_end;
        }

        bool operator!=(const iterator& other) const {
            return !(*this == other);
        }
    };

    explicit sync_iter_stream_iterator(StreamAwaitable&& stream) : stream_(std::move(stream)) {
        // Fetch the first element
        advance();
    }

    iterator begin() {
        return iterator(this);
    }

    iterator end() {
        return iterator(nullptr);  // Sentinel
    }
};

template<stream_awaitable StreamAwaitable>
constexpr auto sync_iter(StreamAwaitable&& stream) {
    return sync_iter_stream_iterator<std::remove_cvref_t<StreamAwaitable>>(std::move(stream));
}

}  // namespace pollcoro
