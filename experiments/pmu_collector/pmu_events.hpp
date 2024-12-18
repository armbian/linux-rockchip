#ifndef PMU_COLLECTOR_HPP__
#define PMU_COLLECTOR_HPP__

#ifndef _GNU_SOURCE
    #define _GNU_SOURCE
#endif

// #include <errno.h>
// #include <fcntl.h>
// #include <unistd.h>
// #include <inttypes.h>
// #include <sys/ioctl.h>
// #include <stdlib.h>
// #include <stdio.h>
// #include <string.h>
// #include <linux/perf_event.h>
// #include <sys/syscall.h>

#include <vector>

namespace PMU
{

struct EventDescriptor
{
    uint32_t type;
    uint32_t perf_id;
};

union EventsReadFormat
{
    char buf[4096];
    struct ReadFormat
    {
        uint64_t num_events;
        uint64_t time_enabled;
        uint64_t time_running;
        struct
        {
            uint64_t value;
            uint64_t id;
        } values[];
    } rd_format;
};

class Event
{
public:
    Event(pid_t pid, EventDescriptor event, int group_fd);
    Event(const Event& event_cp) = default;
    Event(Event&& event_cp) = default;
    ~Event();

    Event& operator=(Event&& event_cp);

    int enable_collection() const;
    int disable_collection(bool close_event = false) const;

private:
    EventDescriptor event_;
    int fd_ = -1;
    uint64_t id_ = -1ULL;

};

class Events
{
public:
    Events(pid_t pid, const std::vector<EventDescriptor>& events);
    Events();
    Events(const Events &event) = delete;
    Events& operator=(Events &&event) = delete;

    void add_events(pid_t pid_proc, const std::vector<EventDescriptor>& events);
    void clear_events();
    size_t get_num_events() const;
    int enable_collection() const;
    int disable_collection(bool close_events = false) const;
    std::vector<float> read_events() const;

    pid_t pid_ = -1;
    std::vector<Event> events_;
};

} // namespace PMU

#endif // PMU_COLLECTOR_HPP__
