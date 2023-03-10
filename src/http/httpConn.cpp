#include "httpConn.h"

bool HttpConn::s_useET;
std::string HttpConn::s_srcDir;
std::atomic<size_t> HttpConn::s_usersCount(0);

HttpConn::HttpConn() {
    m_fd = -1;
    m_addr = { 0 };
    m_isClosed = true;
}

HttpConn::~HttpConn() {
    doClose();
}

void HttpConn::init(int connFd, const sockaddr_in &addr) {
    assert(connFd);

    m_fd = connFd;
    m_addr = addr;

    m_writeBuff.retrieveAll();
    m_readBuff.retrieveAll();

    s_usersCount += 1;
    m_isClosed = false;

    std::string msg = "connection built from: " + std::string(getIp()) + ':' + std::to_string(getPort()) + " - fd: " + std::to_string(m_fd);
    Logger::Instance()->LOG_INFO(msg);
}

/**
 * @brief 连接数据读取
 * 
 * @param readErrno 带出错误
 * @return ssize_t  本轮读取长度
 */
ssize_t HttpConn::read(int* readErrno) {
    ssize_t len = -1;

    do {
        size_t writable = m_readBuff.writableBytes();

        m_iovRead[0].iov_base = m_readBuff.beginWritePtr();
        m_iovRead[0].iov_len = writable;
        m_iovRead[1].iov_base = m_expandedBuff;
        m_iovRead[1].iov_len = EXPANDED_BUFF_SIZE;

        const ssize_t len = readv(m_fd, m_iovRead, 2);
        if (len < 0) {
            *readErrno = errno;
            break;
        }
        else if (len <= writable)
            m_readBuff.hasWritten(len);
        else {
            // 大于buffer长度，扩容存储
            m_readBuff.beenFilled();
            m_readBuff.append(m_expandedBuff, len - writable);
        }

        if (len == 0)
            break;  // read all

    } while(s_useET);

    return len;
}

/**
 * @brief 连接数据写出
 * 
 * @param readErrno 带出错误
 * @return ssize_t  本轮写出长度
 */
ssize_t HttpConn::write(int* readErrno) {
    ssize_t len = -1;

    do {
        len = writev(m_fd, m_iovWrite, m_iovWriteCnt);

        if (len < 0) {
            *readErrno = errno;
            break;
        } 
        else if (m_iovWrite[0].iov_len + m_iovWrite[1].iov_len == 0)    // 所有数据被传输关闭
            break;
        else if (static_cast<size_t>(len) > m_iovWrite[0].iov_len) {    // 保证都是无符号比较
            // 本轮写出数据大于iovWrite的第一个向量
            m_iovWrite[1].iov_base = static_cast<char*>(m_iovWrite[1].iov_base) + (len - m_iovWrite[0].iov_len);
            m_iovWrite[1].iov_len -= len - m_iovWrite[0].iov_len;

            if (m_iovWrite[0].iov_len) 
                m_iovWrite[0].iov_len = 0;
        } else {
            m_iovWrite[0].iov_base = static_cast<char*>(m_iovWrite[0].iov_base) + len;
            m_iovWrite[0].iov_len -= len;
        }
        
    } while(s_useET || bytesToSend() > CONTINUE_SEND_BYTES);    // ET模式 或者 待传输数据量大于阈值

    return len;
}

/**
 * @brief 进一步处理(解析请求、组装响应、准备写出向量)
 * 
 * @return true  1
 * @return false 0
 */
bool HttpConn::process() {
    m_request.init();

    if (m_readBuff.readableBytes() < 0)
        return false;
    else if (m_request.parse(m_readBuff))
        m_response.init(s_srcDir, m_request.path(), m_request.isKeepAlive(), 200);
    else
        m_response.init(s_srcDir, m_request.path(), false, 400);

    m_response.makeResponse(m_writeBuff);   // http响应字符拼接完成 以及 对应资源的内存映射
    
    m_iovWrite[0].iov_base = const_cast<char*>(m_writeBuff.peek());
    m_iovWrite[0].iov_len = m_writeBuff.readableBytes();
    m_iovWriteCnt = 1;

    if (m_response.mmFile() && m_response.mmFileSize()) {
        m_iovWrite[1].iov_base = m_response.mmFile();
        m_iovWrite[1].iov_len = m_response.mmFileSize();
        m_iovWriteCnt = 2;
    }

    return true;
}

/**
 * @brief 连接关闭
 * 
 * @return true  1
 * @return false 0
 */
bool HttpConn::doClose() {
    m_response.unmapFile();

    if (!m_isClosed) {
        m_writeBuff.retrieveAll();
        m_readBuff.retrieveAll();

        close(m_fd);
        m_isClosed = true;

        if (s_usersCount)
            s_usersCount -= 1;

        std::string msg = "connection close from:" + std::string(getIp()) + ':' + std::to_string(getPort()) + " - fd:" + std::to_string(m_fd);
        Logger::Instance()->LOG_INFO(msg);

        return true;
    }

    return false;
}


int HttpConn::getFd() const {
    return m_fd;
}

struct sockaddr_in HttpConn::getAddr() const {
    return m_addr;
}

const char* HttpConn::getIp() const {
    return inet_ntoa(m_addr.sin_addr);
}

int HttpConn::getPort() const {
    return m_addr.sin_port;
}

// 待传输数据长度
const int HttpConn::bytesToSend() const {
    return m_iovWrite[0].iov_len + m_iovWrite[1].iov_len;
}

const bool HttpConn::isKeepAlive() const {
    return m_request.isKeepAlive();
}
