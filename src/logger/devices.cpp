#include "devices.h"

Terminal::Terminal() {
    type_ = _TERMINAL;
}

void Terminal::write(const char* msg) {
    std::cout<< msg;
}

void Terminal::write(const std::string& msg) {
    std::cout<< msg;
}


File::File(std::ofstream& ofs) {
    type_ = _FILE;
    m_ofs = std::move(ofs);
}

File::~File() {
    m_ofs.flush();
    m_ofs.close();
}

void File::write(const char *msg) {
    m_ofs<< msg;
    // m_ofs.flush();
}

void File::write(const std::string& msg) {
    m_ofs<< msg;
    // m_ofs.flush();
}

void File::changeOFS(std::ofstream& new_ofs) {
    m_ofs.close();
    m_ofs = std::move(new_ofs);
}

int File::s_pre_file_rows_left = 0; // 错误思路
