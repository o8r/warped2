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

#define MPI_MSG_TAG 729

class TimeWarpMPICommunicationManager : public TimeWarpCommunicationManager {
public:
    TimeWarpMPICommunicationManager(unsigned int max_buffer_size, unsigned int num_worker_threads) :
        max_buffer_size_(max_buffer_size), num_worker_threads_(num_worker_threads) {}

    unsigned int initialize();

    void finalize();

    unsigned int getNumProcesses();

    unsigned int getID();

    int waitForAllProcesses();

    int sumReduceUint64(uint64_t* send_local, uint64_t* recv_global);

    int gatherUint64(uint64_t* send_local, uint64_t* recv_root);

    void sendMessage(std::unique_ptr<TimeWarpKernelMessage> msg, unsigned int thread_id);

    void sendMessage(std::unique_ptr<TimeWarpKernelMessage> msg);

    bool handleReceives(unsigned int thread_id);

protected:
    unsigned int startReceiveRequests(unsigned int thread_id);

    unsigned int testReceiveRequests(unsigned int thread_id);

    unsigned int testSendRequests(unsigned int thread_id);

    bool isInitiatingThread();

private:
    unsigned int max_buffer_size_;
    unsigned int num_worker_threads_;

    std::shared_ptr<MessageQueue> send_queue_;
    std::shared_ptr<MessageQueue> recv_queue_;

    int my_rank_;
    int num_processes_;
};

struct PendingRequest {
    PendingRequest(std::unique_ptr<uint8_t[]> buffer) :
        buffer_(std::move(buffer)) {}

    std::unique_ptr<uint8_t[]>  buffer_;
    MPI_Request                 request_;
    MPI_Status                  status_;
    int                         flag_;
};

struct MessageQueue {
    MessageQueue(unsigned int max_buffer_size) : max_buffer_size_(max_buffer_size) {}

    unsigned int max_buffer_size_;
    std::unique_ptr<std::vector<std::unique_ptr<PendingRequest>> []>    pending_request_list_;
};


} // namespace warped

#endif

