#ifndef SEQUENTIAL_EVENT_DISPATCHER_HPP
#define SEQUENTIAL_EVENT_DISPATCHER_HPP

#include <memory>
#include <unordered_map>
#include <cassert>

#include "EventDispatcher.hpp"

namespace warped {

class EventStatistics;
class LogicalProcess;

// This class is an EventDispatcher that runs on a single thread and process.
class SequentialEventDispatcher : public EventDispatcher {
public:
    SequentialEventDispatcher(unsigned int max_sim_time, std::unique_ptr<EventStatistics> stats);

    TerminationStatus startSimulation(const std::vector<std::vector<LogicalProcess*>>& lps) override;
    TerminationStatus restart(const std::vector<LogicalProcess*>&, cereal::PortableBinaryInputArchive&,
			      std::chrono::time_point<std::chrono::steady_clock> const&,
			      std::chrono::time_point<std::chrono::steady_clock> const&) override {
      assert(!"SequentialEventDispatcher does not support restarting from checkpoints");
      throw std::runtime_error("SequentialEventDispatcher does not support restarting from checkpoints");
      return TS_ERROR;
    }
    FileStream& getFileStream(LogicalProcess* lp, const std::string& filename,
        std::ios_base::openmode mode, std::shared_ptr<Event> this_event);

private:
    unsigned int current_sim_time_ = 0;

    std::unique_ptr<EventStatistics> stats_;

    std::unordered_map<std::string, std::unique_ptr<FileStream, FileStreamDeleter>>
        file_stream_by_filename_;

    std::unordered_map<std::string, LogicalProcess*> lp_by_filename_;

};

} // namespace warped

#endif
