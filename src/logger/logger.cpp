#include "logger.h"
#include "devices.h"

Logger::Logger() {
    m_initilized = false;
    m_level = MsgLevel::_NONE;
    m_path = nullptr;
    m_suffix = nullptr;
};

Logger::~Logger() {
    if (m_writeThread && m_writeThread->joinable()) {
        while (!m_blockingDeq->empty()) 
            m_blockingDeq->flush();

        m_blockingDeq->clear();
        m_writeThread->join();
    }
}

Logger*     Logger::s_logger    = nullptr;
std::mutex  Logger::m_mtx;

Logger* Logger::Instance() {
    if (s_logger == nullptr) {
        {
            std::lock_guard<std::mutex> locker(m_mtx);
            s_logger = new Logger();
        }
    }

    return s_logger;
}

void Logger::Destroy() {
    if (s_logger == nullptr)
        return;
    
    delete s_logger;
}

void Logger::init(
    MsgLevel default_level = MsgLevel::_INFO,
    LoggerDevice default_device = LoggerDevice::_BOTH,
    const char* path = "./log",
    const char* suffix = ".log",
    int dequeCapacity = 1024
) {
    assert(dequeCapacity > 0 && !m_initilized);

    m_level = default_level;
    m_device = default_device; 
    m_path = path;
    m_suffix = suffix;

    if (!m_blockingDeq)         // blocking deque ready
        m_blockingDeq = new BlockingDeque<std::string>(dequeCapacity);
        
    char fileName[LOG_FILE_NAME_MAX_LEN];
    fetchFileName(fileName);    // get log file name

    std::ofstream ofs;
    openLogFile(ofs, fileName);

    std::unique_ptr<Device> device_T = std::make_unique<Terminal>();
    std::unique_ptr<Device> device_F = std::make_unique<File>(ofs);

    if (m_device == LoggerDevice::_TERMINAL)
        m_devices.push_back(std::move(device_T));
    else if (m_device == LoggerDevice::_FILE)
        m_devices.push_back(std::move(device_F));
    else if (m_device == LoggerDevice::_BOTH) {
        m_devices.push_back(std::move(device_T));
        m_devices.push_back(std::move(device_F));
    }   // output devices ready

    if (!m_writeThread) {
        m_writeThread = std::make_unique<std::thread>(raiseWriteThread);    // write thread ready
        // m_writeThread->detach();
    }

    m_initilized = true;
}

void Logger::write(MsgLevel level, const char* msg) {
    assert(m_initilized);

    if (level > m_level || level == _NONE)
        return;

    // reformat msg to standard msg
    std::string s_(msg);
    formatMsg(level, s_);
    m_blockingDeq->pushFront(s_);
}

void Logger::write(MsgLevel level, std::string& msg) {
    assert(m_initilized);

    if (level > m_level || level == _NONE)
        return;

    formatMsg(level, msg);
    m_blockingDeq->pushFront(msg);
}

void Logger::formatMsg(MsgLevel level, std::string& msg) {
    msg.append(1, '\n');
    
    switch(level) {
        case _NONE:
            break;
        case _ERROR:
            msg.insert(0, " [ERROR]: ");
            break;
        case _WARNING:
            msg.insert(0, " [WARNING]: ");
            break;
        case _DEBUG:
            msg.insert(0, " [DEBUG]: ");
            break;
        default:
            msg.insert(0, " [INFO]: ");
            break;
    }

    fetchNowTime();
    msg.insert(0, m_nowTime);
}

void Logger::fetchFileName(char* fileName) const {
    time_t now_time;
    struct tm* t;

    time(&now_time);
    t = localtime(&now_time);

    snprintf(fileName, 256, "%s/%04d_%02d_%02d%s",
        m_path, t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, m_suffix);
}

void Logger::fetchNowTime() {
    time_t now_time;
    struct tm* t;

    time(&now_time);
    t = localtime(&now_time);

    int _year = t->tm_year + 1900;
    int _month = t->tm_mon + 1;
    int _day = t->tm_mday;
    snprintf(m_nowTime, 56, "%04d-%02d-%02d %02d:%02d:%02d %s",
    _year, _month, _day, t->tm_hour, t->tm_min, t->tm_sec, t->tm_zone);

    m_new_date = {_year, _month, _day};
}

void Logger::openLogFile(std::ofstream& ofs, const char* fileName) {
    ofs.open(fileName, std::ios::app);
    if (!ofs.good()) {
        mkdir(m_path, 0777);
        ofs.open(fileName, std::ios::app);
    }

    assert(ofs.good());         // make log file exists
}

void Logger::writeThreadJobs() {
    std::string msg;

    while (m_blockingDeq->pop(msg)) {
        std::lock_guard<std::mutex> locker(m_mtx);

        for (auto& device : m_devices)
            device->write(msg);

        // check if it is a new day
        // ??????????????????????????? - ?????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????
        // ?????????????????????tps?????????????????????????????????????????????????????????
        // ???????????????: ??????devices?????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????
        if (m_new_date != m_old_date && (m_device == _FILE || m_device == _BOTH)) {
            std::ofstream ofs;
            char fileName[LOG_FILE_NAME_MAX_LEN];

            fetchFileName(fileName);
            openLogFile(ofs, fileName);

            for (auto& device : m_devices) {
                if (device->type_ == _FILE) {
                    device->changeOFS(ofs);
                    break;
                }
            }
        }

        m_old_date = m_new_date; 
    }
}

void Logger::raiseWriteThread() {
    Logger::Instance()->writeThreadJobs();
}

void Logger::flushAll() {
    for (auto& device : m_devices) {
        if (device)
            device->flush();
    }
}


// ----
void Logger::LOG_ERROR(const char* msg) {
    write(_ERROR, msg);
}
void Logger::LOG_ERROR(std::string& msg) {
    write(_ERROR, msg);
}

void Logger::LOG_WARNING(const char* msg) {
    write(_WARNING, msg);
}
void Logger::LOG_WARNING(std::string& msg) {
    write(_WARNING, msg);
}

void Logger::LOG_DEBUG(const char* msg) {
    write(_DEBUG, msg);
}
void Logger::LOG_DEBUG(std::string& msg) {
    write(_DEBUG, msg);
}

void Logger::LOG_INFO(const char* msg) {
    write(_INFO, msg);
}
void Logger::LOG_INFO(std::string& msg) {
    write(_INFO, msg);
}

const std::tuple<std::string, std::string, std::string> Logger::loggerDesc() const {
    std::string levelStr = "";
    std::string deviceStr = "";

    switch (m_level) {
        case _NONE:
            levelStr += "??????";
            break;
        case _ERROR:
            levelStr += "????????????";
            break;
        case _WARNING:
            levelStr += "????????????";
            break;
        case _DEBUG:
            levelStr += "????????????";
            break;
        case _INFO:
            levelStr += "????????????";
            break;
        default:
            break;
    }

    switch (m_device) {
        case _TERMINAL:
            deviceStr += "?????????";
            break;
        case _FILE:
            deviceStr += "?????????";
            break;
        case _BOTH:
            deviceStr += "???????????????";
            break;
        default:
            break;
    }

    return std::make_tuple(levelStr, deviceStr, m_path);
}
