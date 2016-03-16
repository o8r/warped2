#ifndef SYNCHRONOUS_GVT_MANAGER_HPP
#define SYNCHRONOUS_GVT_MANAGER_HPP

#include <memory> // for shared_ptr
#include <atomic>

#include <pthread.h>

#include "TimeWarpEventDispatcher.hpp"
#include "TimeWarpGVTManager.hpp"
#include "serialization.hpp"

namespace warped {

class TimeWarpSynchronousGVTManager : public TimeWarpGVTManager {
public:
    TimeWarpSynchronousGVTManager(std::shared_ptr<TimeWarpCommunicationManager> comm_manager,
        unsigned int period, unsigned int num_worker_threads)
        : TimeWarpGVTManager(comm_manager, period, num_worker_threads) {}

    void initialize() override;

    bool readyToStart();

    void progressGVT();

    void receiveEventUpdate(std::shared_ptr<Event>& event, Color color);

    Color sendEventUpdate(std::shared_ptr<Event>& event);

    bool gvtUpdated();

    int64_t getMessageCount() {
        return white_msg_count_.load();
    }

    void reportThreadMin(unsigned int timestamp, unsigned int thread_id,
                                 unsigned int local_gvt_flag);

    void reportThreadSendMin(unsigned int timestamp, unsigned int thread_id);

    unsigned int getLocalGVTFlag();

protected:
    bool gvt_updated_ = false;

    std::atomic<int64_t> white_msg_count_ = ATOMIC_VAR_INIT(0);

    std::atomic<Color> color_ = ATOMIC_VAR_INIT(Color::WHITE);

    std::atomic<unsigned int> local_gvt_flag_ = ATOMIC_VAR_INIT(0);

    std::unique_ptr<unsigned int []> local_min_;

    std::unique_ptr<unsigned int []> send_min_;

    unsigned int recv_min_;

    pthread_barrier_t min_report_barrier_;

    friend class cereal::access;
    template <typename Archive>
    void save(Archive& ar) const {
      ar(cereal::base_class<TimeWarpGVTManager>(this));
      ar(gvt_updated_);
      ar(white_msg_count_.load());
      ar(color_.load());
      ar(local_gvt_flag_.load());

      for (unsigned int i=0; i<num_worker_threads_+1; ++i) ar(local_min_[i]);
      for (unsigned int i=0; i<num_worker_threads_+1; ++i) ar(send_min_[i]);

      ar(recv_min_);
    }
    template <typename Archive>
    void load(Archive& ar) {
      auto prev_num_worker_threads = num_worker_threads_;

      ar(cereal::base_class<TimeWarpGVTManager>(this));
      
      int64_t white_msg_count;
      ar(white_msg_count);
      white_msg_count_.store(white_msg_count);

      Color color;
      ar(color);
      color_.store(color);

      unsigned int local_gvt_flag;
      ar(local_gvt_flag);
      local_gvt_flag_.store(local_gvt_flag);

      if (num_worker_threads_ != prev_num_worker_threads) {
	pthread_barrier_destroy(&min_report_barrier_);
	pthread_barrier_init(&min_report_barrier_, NULL, num_worker_threads_+1);

	local_min_ = make_unique<unsigned int[]>(num_worker_threads_ + 1);
	send_min_ = make_unique<unsigned int[]>(num_worker_threads_ + 1);
      }

      for (unsigned int i=0; i<num_worker_threads_+1; ++i) ar(local_min_[i]);
      for (unsigned int i=0; i<num_worker_threads_+1; ++i) ar(send_min_[i]);

      ar(recv_min_);
    }
    template <typename Archive>
    static void load_and_construct(Archive& ar, cereal::construct<TimeWarpSynchronousGVTManager>& construct) {
      construct(nullptr, 0, 0);

      ar(cereal::base_class<TimeWarpGVTManager>(construct.ptr()));

      int64_t white_msg_count;
      ar(white_msg_count);
      construct->white_msg_count_.store(white_msg_count);

      Color color;
      ar(color);
      construct->color_.store(color);

      unsigned int local_gvt_flag;
      ar(local_gvt_flag);
      construct->local_gvt_flag_.store(local_gvt_flag);

      construct->initialize();

      for (unsigned int i=0; i<construct->num_worker_threads_ + 1; ++i) ar(construct->local_min_[i]);
      for (unsigned int i=0; i<construct->num_worker_threads_ + 1; ++i) ar(construct->send_min_[i]);

      ar(construct->recv_min_);
    }
};

} // warped namespace

#endif
