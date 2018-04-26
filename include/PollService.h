#pragma once

#include "Poller.h"

#include <map>
#include <mutex>

namespace Chili {

/**
 * Polling as a service; i.e., attach different handlers
 * to specific file polling operations.
 */
class PollService {
public:
    /**
     * Creates a new service with the specified number
     * of underlying handler execution threads.
     */
    PollService(int threads);
    ~PollService();

    /**
     * Poll a specific file for events using a custom handler.
     */
    std::future<std::shared_ptr<FileStream>> Poll(std::shared_ptr<FileStream>,
                                                  Poller::EventHandler,
                                                  int events = Poller::Events::NotifyAll);

private:
    struct Task {
        std::shared_ptr<FileStream> _file;
        Poller::EventHandler _handler;
        std::promise<std::shared_ptr<FileStream>> _promise;
    };

    Poller _poller;
    std::map<const void*, Task> _tasks;
    std::mutex _mutex;
};

} // namespace Chili

