#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <dirent.h>
#include <memory>
#include <sys/stat.h>
#include <boost/asio.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/locale.hpp>

using boost::asio::io_context;
using boost::asio::ip::tcp;
using boost::system::error_code;
using std::cerr;
using std::cout;
using std::endl;
using std::string;
using std::thread;
using std::unique_ptr;
using std::make_unique;

int init_log()
{
    int ret = 0;
    struct stat st;

    if ((stat("./logs", &st) != 0) || ((st.st_mode & S_IFDIR) == 0))
    {
        ret = mkdir("./logs");
        if (ret != 0)
        {
            cerr << "mkdir ./logs failed" << endl;
            return -1;
        }
    }

    boost::log::add_file_log
    (
        boost::log::keywords::file_name = "./logs/sync_tcp_server.log", // 日志文件名
        boost::log::keywords::rotation_size = 10 * 1024 * 1024, // 日志文件大小
        boost::log::keywords::time_based_rotation = boost::log::sinks::file::rotation_at_time_point(0, 0, 0), // 日志文件时间
        boost::log::keywords::format = "[%TimeStamp% %ThreadID% %Severity%]: %Message%" // 日志格式
    );

    boost::log::add_console_log(std::cout, boost::log::keywords::format = "[%TimeStamp% %ThreadID% %Severity%]: %Message%");

    boost::log::add_common_attributes();

    return 0;
}

void sync_tcp_server_thread(const int ip_version)
{
    BOOST_LOG_TRIVIAL(info) << "ipv" << ip_version << "_thread" << " start.";
    string send("hello from sync tcp server");

    io_context io;
    unique_ptr<tcp::acceptor> acceptor;

    switch (ip_version)
    {
    case 4:
    {
        acceptor = make_unique<tcp::acceptor>(io, tcp::endpoint(tcp::v4(), 20712)); // 监听ipv4
        break;
    }
    case 6:
    {
        acceptor = make_unique<tcp::acceptor>(io, tcp::endpoint(tcp::v6(), 20712)); // 监听ipv6
        break;
    }
    default: // 默认选择ipv4
    {
        BOOST_LOG_TRIVIAL(warning) << "invalid ip version: " << ip_version << ", use ipv4.";
        acceptor = make_unique<tcp::acceptor>(io, tcp::endpoint(tcp::v4(), 20712)); // 监听ipv4
    }
    }

    while (true)
    {
        boost::log::core::get()->flush(); // 刷新日志缓冲区

        error_code ec;
        tcp::socket socket(io);

        // 连接
        acceptor->accept(socket, ec);
        if (ec)
        {
            BOOST_LOG_TRIVIAL(error) << "accept error: " << boost::locale::conv::between(ec.message(), "UTF-8", "GBK");
            socket.close();
            continue;
        }
        BOOST_LOG_TRIVIAL(info) << "accept a new connection";

        // 发送
        boost::asio::write(socket, boost::asio::buffer(send), ec);
        if (ec)
        {
            BOOST_LOG_TRIVIAL(error) << "write error: " << boost::locale::conv::between(ec.message(), "UTF-8", "GBK");
            socket.close();
            continue;
        }
        BOOST_LOG_TRIVIAL(info) << "send success";

        // 断开连接
        socket.close();
        BOOST_LOG_TRIVIAL(info) << "close a connection";
    }
}

// 同步tcp服务器测试
int main()
{
    int ret = 0;

    ret = init_log();
    if (ret != 0)
    {
        cerr << "init log failed" << endl;
        return -1;
    }

    BOOST_LOG_TRIVIAL(info) << "tcp sync server start.";

    thread thread_v4(sync_tcp_server_thread, 4);
    thread thread_v6(sync_tcp_server_thread, 6);

    thread_v4.join();
    thread_v6.join();

    return 0;
}
