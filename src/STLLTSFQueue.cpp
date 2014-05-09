#include "STLLTSFQueue.hpp"

#include <queue>
#include <utility>

#include "Event.hpp"

namespace warped {

STLLTSFQueue::STLLTSFQueue()
    : queue_([](Event* lhs, Event* rhs) {
    return lhs->get_receive_time() > rhs->get_receive_time();
}) {}

bool STLLTSFQueue::empty() const {
    return queue_.empty();
}

unsigned int STLLTSFQueue::size() const {
    return queue_.size();
}

Event* STLLTSFQueue::peek() const {
    if (queue_.empty()) {
        return nullptr;
    }
    return queue_.top();
}

std::unique_ptr<Event> STLLTSFQueue::pop() {
    auto ret = queue_.top();
    queue_.pop();
    return std::unique_ptr<Event>{ret};
}

void STLLTSFQueue::push(std::unique_ptr<Event> event) {
    queue_.push(event.release());
}

} // namespace warped
