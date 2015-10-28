#include <cstring> // for memcpy
#include <algorithm> // for std::remove_if
#include <cstdint>  // for uint8_t type
#include <cassert>

#include "TimeWarpMPICommunicationManager.hpp"
#include "TimeWarpEventDispatcher.hpp"          // for EventMessage
#include "TimeWarpMatternGVTManager.hpp"
#include "utility/memory.hpp"
#include "utility/warnings.hpp"

namespace warped {

TicketLock serialization_lock;

unsigned int TimeWarpMPICommunicationManager::initialize() {

    // MPI_Init requires command line arguments, but doesn't use them. Just give
    // it an empty vector.
    int argc = 0;
    char** argv = new char*[1];
    argv[0] = NULL;
    int provided;

    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);

    delete [] argv;

    if (provided < MPI_THREAD_MULTIPLE) {
        throw std::runtime_error("MPI_THREAD_MULTIPLE is not supported by the MPI \
implementation you are currently using.");
    }

    send_queue_ = std::make_shared<MPISendQueue>(max_buffer_size_);
    recv_queue_ = std::make_shared<MPIRecvQueue>(max_buffer_size_);

    send_queue_->request_list_.reserve(50000);
    send_queue_->buffer_list_.reserve(50000);

    recv_queue_->request_list_.reserve(50000);
    recv_queue_->buffer_list_.reserve(50000);

    return getNumProcesses();
}

void TimeWarpMPICommunicationManager::finalize() {
    MPI_Finalize();
}

unsigned int TimeWarpMPICommunicationManager::getNumProcesses() {
    int size = 0;
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    return size;
}

unsigned int TimeWarpMPICommunicationManager::getID() {
    int id = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &id);
    return id;
}

int TimeWarpMPICommunicationManager::waitForAllProcesses() {
    assert(isInitiatingThread());
    return MPI_Barrier(MPI_COMM_WORLD);
}

bool TimeWarpMPICommunicationManager::isInitiatingThread() {
    int flag = 0;
    MPI_Is_thread_main(&flag);
    return flag;
}

int
TimeWarpMPICommunicationManager::sumReduceUint64(uint64_t *send_local, uint64_t *recv_global) {
    return MPI_Reduce(send_local, recv_global, 1, MPI_UINT64_T, MPI_SUM, 0, MPI_COMM_WORLD);
}

int
TimeWarpMPICommunicationManager::gatherUint64(uint64_t *send_local, uint64_t *recv_root) {
    return MPI_Gather(send_local, 1, MPI_UINT64_T, recv_root, 1, MPI_UINT64_T, 0, MPI_COMM_WORLD);
}

void TimeWarpMPICommunicationManager::sendMessage(std::unique_ptr<TimeWarpKernelMessage> msg) {
    send_queue_->lock_.lock();

    unsigned int receiver_id = msg->receiver_id;

    std::ostringstream oss;
    {
        serialization_lock.lock();
        cereal::PortableBinaryOutputArchive oarchive(oss);
        oarchive(std::move(msg));
        serialization_lock.unlock();
    }

    send_queue_->request_list_.emplace_back();
    send_queue_->buffer_list_.emplace_back(new uint8_t[max_buffer_size_]);

    MPI_Buffer_attach(send_queue_->buffer_list_.back().get(), max_buffer_size_);
    if (MPI_Ibsend(
            const_cast<char*>(oss.str().c_str()),
            oss.str().length()+1,
            MPI_BYTE,
            receiver_id,
            MPI_DATA_TAG,
            MPI_COMM_WORLD,
            &send_queue_->request_list_.back()) != MPI_SUCCESS) {
    }

    send_queue_->lock_.unlock();
}

void TimeWarpMPICommunicationManager::handleMessageQueues() {
    recv_queue_->testRequests();
    send_queue_->testRequests();
    startReceiveRequests();
}

std::unique_ptr<TimeWarpKernelMessage> TimeWarpMPICommunicationManager::getMessage() {
    if (recv_queue_->msg_list_.empty()) {
        return nullptr;
    }
    auto msg = std::move(recv_queue_->msg_list_.front());
    recv_queue_->msg_list_.pop_front();

    return std::move(msg);
}

unsigned int TimeWarpMPICommunicationManager::startReceiveRequests() {
    int flag = 0;
    MPI_Status status;
    unsigned int requests = 0;

    while (true) {

        MPI_Iprobe(
            MPI_ANY_SOURCE,
            MPI_ANY_TAG,
            MPI_COMM_WORLD,
            &flag,
            &status);

        if (flag) {
            recv_queue_->buffer_list_.emplace_back(new uint8_t[max_buffer_size_]);
            recv_queue_->request_list_.emplace_back();

            if (MPI_Irecv(
                    recv_queue_->buffer_list_.back().get(),
                    max_buffer_size_,
                    MPI_BYTE,
                    MPI_ANY_SOURCE,
                    MPI_DATA_TAG,
                    MPI_COMM_WORLD,
                    &recv_queue_->request_list_.back()) != MPI_SUCCESS) {

                return requests;
            }
        } else {
            break;
        }

        requests++;
    }

    return requests;
}

unsigned int MPISendQueue::testRequests() {
    int ready = 0;
    unsigned int n;

    lock_.lock();

    if (request_list_.empty()) {
        lock_.unlock();
        return 0;
    }

    std::vector<int> index_list(request_list_.size());

    if (MPI_Testsome(
            request_list_.size(),
            &request_list_[0],
            &ready,
            &index_list[0],
            MPI_STATUSES_IGNORE) != MPI_SUCCESS) {
        lock_.unlock();
        return 0;
    }

    if (ready < 1) {
        lock_.unlock();
        return 0;
    }

    std::reverse(index_list.begin(), index_list.end());
    for (int i = 0; i < ready; i++) {
        n = index_list[i];

        int size;
        auto p = buffer_list_[n].get();
        MPI_Buffer_detach(&p, &size);
        buffer_list_.erase(buffer_list_.begin() + n);

        request_list_.erase(request_list_.begin() + n);
    }

    lock_.unlock();

    return ready;
}

unsigned int MPIRecvQueue::testRequests() {
    int ready = 0;
    unsigned int n;

    if (request_list_.empty())
        return 0;

    std::vector<int> index_list(request_list_.size());
    std::vector<MPI_Status> status_list(request_list_.size());

    if (MPI_Testsome(
            request_list_.size(),
            &request_list_[0],
            &ready,
            &index_list[0],
            &status_list[0]) != MPI_SUCCESS) {
        return 0;
    }

    if (ready < 1)
        return 0;

    for (int i = 0; i < ready; i++) {
        n = index_list[i];

        std::unique_ptr<TimeWarpKernelMessage> msg = nullptr;

        int count;
        MPI_Get_count(&status_list[n], MPI_BYTE, &count);
        std::cout << count << std::endl;

        std::istringstream iss(std::string(reinterpret_cast<char*>(buffer_list_[n].get()), count));
        {
            serialization_lock.lock();
            cereal::PortableBinaryInputArchive iarchive(iss);
            iarchive(msg);
            serialization_lock.unlock();
        }
        msg_list_.push_back(std::move(msg));
    }

    std::reverse(index_list.begin(), index_list.end());
    for (int i = 0; i < ready; i++) {
        n = index_list[i];
        buffer_list_.erase(buffer_list_.begin() + n);
        request_list_.erase(request_list_.begin() + n);
    }

    return ready;
}

} // namespace warped

