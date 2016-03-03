#ifndef MPI_COMMUNICATION_MANAGER_HPP
#define MPI_COMMUNICATION_MANAGER_HPP

#include <mpi.h>
#include <vector>
#include <cstdint>
#include <mutex>

#include "TimeWarpCommunicationManager.hpp"
#include "TimeWarpKernelMessage.hpp"

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
};

struct PendingRequest {
    PendingRequest(std::unique_ptr<uint8_t[]> buffer, unsigned int count) :
        buffer_(std::move(buffer)), count_(count) {}

    std::unique_ptr<uint8_t[]> buffer_;
    MPI_Request request_;
    int flag_;
    MPI_Status status_;
    int count_;
};

struct MessageQueue {
    MessageQueue(unsigned int max_buffer_size) :
        max_buffer_size_(max_buffer_size) {}

    unsigned int max_buffer_size_;

    std::deque<std::unique_ptr<TimeWarpKernelMessage>>  msg_list_;
    std::mutex msg_list_lock_;

    std::vector<std::unique_ptr<PendingRequest>> pending_request_list_;
};

} // namespace warped

#endif

