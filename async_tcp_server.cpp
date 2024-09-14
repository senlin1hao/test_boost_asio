#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <memory>
#include <dirent.h>
#include <sys/stat.h>
#include <boost/asio.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/bind.hpp>
#include <boost/locale.hpp>

using boost::asio::io_context;
using boost::asio::ip::tcp;
using boost::system::error_code;
using std::cerr;
using std::cout;
using std::endl;
using std::string;
using std::thread;
using std::shared_ptr;
using std::make_shared;

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
        boost::log::keywords::file_name = "./logs/async_tcp_server.log", // 日志文件名
        boost::log::keywords::rotation_size = 10 * 1024 * 1024, // 日志文件大小
        boost::log::keywords::time_based_rotation = boost::log::sinks::file::rotation_at_time_point(0, 0, 0), // 日志文件时间
        boost::log::keywords::format = "[%TimeStamp% %ThreadID% %Severity%]: %Message%" // 日志格式
    );

    boost::log::add_console_log(std::cout, boost::log::keywords::format = "[%TimeStamp% %ThreadID% %Severity%]: %Message%");

    boost::log::add_common_attributes();

    return 0;
}

void async_write_handler(io_context& io, shared_ptr<tcp::socket>& socket, const boost::system::error_code& ec)
{
    if (ec)
    {
        BOOST_LOG_TRIVIAL(error) << "write error: " << boost::locale::conv::between(ec.message(), "UTF-8", "GBK");
        return;
    }
    BOOST_LOG_TRIVIAL(info) << "send success";

    // 断开连接
    socket->close();
    BOOST_LOG_TRIVIAL(info) << "close a connection.";

    return;
}

void async_accept_handler(io_context& io, shared_ptr<tcp::socket>& socket, const boost::system::error_code& ec)
{
    if (ec)
    {
        BOOST_LOG_TRIVIAL(error) << "accept error: " << boost::locale::conv::between(ec.message(), "UTF-8", "GBK");
        return;
    }

    BOOST_LOG_TRIVIAL(info) << "accept a new connection.";

    // 发送
    string send("hello from async tcp server");
    socket->async_write_some(boost::asio::buffer(send), boost::bind(&async_write_handler, std::ref(io), std::ref(socket), boost::asio::placeholders::error));

    return;
}

void async_tcp_server_thread(const int ip_version)
{
    BOOST_LOG_TRIVIAL(info) << "ipv" << ip_version << "_thread" << " start.";
    string send("hello from async tcp server");

    while (true)
    {
        boost::log::core::get()->flush(); // 刷新日志缓冲区

        io_context io;
        switch (ip_version)
        {
        case 4: // ipv4连接
        {
            tcp::acceptor acceptor_v4(io, tcp::endpoint(tcp::v4(), 20712)); // 监听ipv4
            shared_ptr<tcp::socket> socket_v4 = make_shared<tcp::socket>(io);

            acceptor_v4.async_accept(*socket_v4, boost::bind(&async_accept_handler, std::ref(io), std::ref(socket_v4), boost::asio::placeholders::error));
            io.run();
            break;
        }
        case 6: // ipv6连接
        {
            tcp::acceptor acceptor_v6(io, tcp::endpoint(tcp::v6(), 20712)); // 监听ipv6
            shared_ptr<tcp::socket> socket_v6 = make_shared<tcp::socket>(io);

            acceptor_v6.async_accept(*socket_v6, boost::bind(&async_accept_handler, std::ref(io), std::ref(socket_v6), boost::asio::placeholders::error));
            io.run();
            break;
        }

        default: // 默认选择ipv4连接
        {
            BOOST_LOG_TRIVIAL(warning) << "invalid ip version: " << ip_version << ", use ipv4";
            tcp::acceptor acceptor_v4(io, tcp::endpoint(tcp::v4(), 20712)); // 监听ipv4
            shared_ptr<tcp::socket> socket_v4 = make_shared<tcp::socket>(io);

            acceptor_v4.async_accept(*socket_v4, boost::bind(&async_accept_handler, std::ref(io), std::ref(socket_v4), boost::asio::placeholders::error));
            io.run();
            break;
        }
        }
    }
}

int main()
{
    int ret = 0;
    ret = init_log();
    if (ret != 0)
    {
        cerr << "init log failed" << endl;
        return -1;
    }

    BOOST_LOG_TRIVIAL(info) << "tcp async server start.";

    thread thread_v4(async_tcp_server_thread, 4);
    thread thread_v6(async_tcp_server_thread, 6);

    thread_v4.join();
    thread_v6.join();

    return 0;
}
