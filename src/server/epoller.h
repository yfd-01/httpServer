/*
    epoll操作封装
*/

#ifndef _EPOLLER_H
#define _EPOLLER_H

#include <sys/epoll.h>
#include <vector>
#include <unistd.h>     // close
#include <cassert>

class Epoller {
public:
    Epoller(int maxEvent = 1024);
    ~Epoller();

    bool addFd(int fd, uint32_t events);
    bool modFd(int fd, uint32_t events);
    bool delFd(int fd);

    int wait(int timeout);
    int getFd(int i) const;
    uint32_t getEvents(int i) const;
private:
    int m_epoll_fd;
    std::vector<struct epoll_event> m_events;
};

#endif  // _EPOLLER_H