#ifndef MPI_COMMUNICATION_MANAGER_HPP
#define MPI_COMMUNICATION_MANAGER_HPP

#include <mpi.h>
#include <vector>
#include <cstdint>
#include <mutex>

#include "TimeWarpCommunicationManager.hpp"
#include "TimeWarpKernelMessage.hpp"
#include "serialization.hpp"

namespace warped {

struct MessageQueue;
struct MPISendQueue;
struct MPIRecvQueue;

#define MPI_DATA_TAG        729

  namespace detail {
    template <typename T> struct MpiDataType;
    template <> struct MpiDataType<uint64_t> { static constexpr MPI_Datatype value = MPI_UINT64_T; };
    template <> struct MpiDataType<double> { static constexpr MPI_Datatype value = MPI_DOUBLE; };
  }  // detail

class TimeWarpMPICommunicationManager : public TimeWarpCommunicationManager {
public:
    TimeWarpMPICommunicationManager(unsigned int max_buffer_size, unsigned max_aggregate) :
        max_buffer_size_(max_buffer_size*max_aggregate), max_aggregate_(max_aggregate) {}

    unsigned int initialize();
    void finalize();
    unsigned int getNumProcesses();
    unsigned int getID();
    int waitForAllProcesses();

    void insertMessage(std::unique_ptr<TimeWarpKernelMessage> msg);
    void handleMessages();
    void flushMessages();

    int sumReduceUint64(uint64_t* send_local, uint64_t* recv_global);
    int gatherUint64(uint64_t* send_local, uint64_t* recv_root);
    double gatherDouble(double* send_local, double* recv_root) { return gather(send_local, recv_root); }
    int sumAllReduceInt64(int64_t* send_local, int64_t* recv_global);
    int minAllReduceUint(unsigned int* send_local, unsigned int* recv_global);

protected:
    void packAndSend(unsigned int receiver_id);

    unsigned int startSendRequests();
    unsigned int startReceiveRequests();
    unsigned int testSendRequests();
    unsigned int testReceiveRequests();

    bool isInitiatingThread();

private:
    template <typename T>
    T gather(T* send_local, T* recv_root) {
      return MPI_Gather(send_local, 1, detail::MpiDataType<T>::value, recv_root, 1, detail::MpiDataType<T>::value, 0, MPI_COMM_WORLD);
    }

    unsigned int max_buffer_size_;
    unsigned int max_aggregate_;

    int num_processes_;
    int my_rank_;

    std::unique_ptr<std::list<std::unique_ptr<TimeWarpKernelMessage>>[]> aggregate_messages_;
    std::unique_ptr<unsigned int[]> aggregate_message_count_by_receiver_ = 0;

    std::shared_ptr<MessageQueue> send_queue_;
    std::shared_ptr<MessageQueue> recv_queue_;

    friend class cereal::access;
    template <typename Archive>
    void save(Archive& ar) const {
      ar(max_buffer_size_, max_aggregate_,
	 cereal::base_class<TimeWarpCommunicationManager>(this),
	 num_processes_, my_rank_);

      for (int i=0; i<num_processes_; ++i)
	ar(aggregate_messages_[i]);
      for (int i=0; i<num_processes_; ++i)
	ar(aggregate_message_count_by_receiver_[i]);

      ar(send_queue_, recv_queue_);
    }
    template <typename  Archive>
    void load(Archive& ar) {
      auto prev_num_processes = num_processes_;

      ar(max_buffer_size_, max_aggregate_, 
	 cereal::base_class<TimeWarpCommunicationManager>(this),
	 num_processes_, my_rank_);

      if (num_processes_ != prev_num_processes) {
	aggregate_messages_ = make_unique<std::list<std::unique_ptr<TimeWarpKernelMessage> >[]>(num_processes_);
	aggregate_message_count_by_receiver_ = make_unique<unsigned int[]>(num_processes_);
      }

      for (int i=0; i<num_processes_; ++i)
	ar(aggregate_messages_[i]);
      for (int i=0; i<num_processes_; ++i)
	ar(aggregate_message_count_by_receiver_[i]);

      ar(send_queue_, recv_queue_);
    }
    template <typename Archive>
    static void load_and_construct(Archive& ar, cereal::construct<TimeWarpMPICommunicationManager>& construct) {
      unsigned int max_buffer_size, max_aggregate;

      ar(max_buffer_size, max_aggregate);
      construct(max_buffer_size, max_aggregate);
      ar(cereal::base_class<TimeWarpCommunicationManager>(construct.ptr()));
      ar(construct->num_processes_, construct->my_rank_);

      construct->aggregate_messages_ = make_unique<std::list<std::unique_ptr<TimeWarpKernelMessage> >[]>(construct->num_processes_);
      for (int i=0; i<construct->num_processes_; ++i)
	ar(construct->aggregate_messages_[i]);
      construct->aggregate_message_count_by_receiver_ = make_unique<unsigned int[]>(construct->num_processes_);
      for (int i=0; i<construct->num_processes_; ++i)
	ar(construct->aggregate_message_count_by_receiver_[i]);

      ar(construct->send_queue_, construct->recv_queue_);
    }
};

struct PendingRequest {
    PendingRequest(std::unique_ptr<uint8_t[]> buffer, std::size_t size, unsigned int count) :
      buffer_(std::move(buffer)), size_(size), count_(count) {}

    std::unique_ptr<uint8_t[]> buffer_;
    std::size_t size_;
    MPI_Request request_;
    int flag_;
    MPI_Status status_;
    int count_;

    template <typename Archive>
    void serialize(Archive& ar) {
      ar(size_, count_);
      for (std::size_t i=0; i<size_; ++i) ar(buffer_[i]);
      ar(flag_);
    }
    template <typename Archive>
    static void load_and_construct(Archive& ar, cereal::construct<PendingRequest>& construct) {
      std::size_t size;
      int count;
      ar(size, count);

      construct(std::move(make_unique<uint8_t[]>(size)), size, count);

      for (std::size_t i=0; i<size; ++i) ar(construct->buffer_[i]);
      
      ar(construct->flag_);
    }
};

struct MessageQueue {
    MessageQueue(unsigned int max_buffer_size) :
        max_buffer_size_(max_buffer_size) {}

    unsigned int max_buffer_size_;

    std::deque<std::unique_ptr<TimeWarpKernelMessage>>  msg_list_;
    std::mutex msg_list_lock_;

    std::vector<std::unique_ptr<PendingRequest>> pending_request_list_;

    template <typename Archive>
    void serialize(Archive& ar) {
      ar(max_buffer_size_, msg_list_, pending_request_list_);
    }
    template <typename Archive>
    static void load_and_construct(Archive& ar, cereal::construct<MessageQueue>& construct) {
      unsigned int max_buffer_size;
      ar(max_buffer_size);

      construct(max_buffer_size);

      ar(construct->msg_list_, construct->pending_request_list_);
    }
};

} // namespace warped

#endif

