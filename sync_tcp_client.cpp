#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <vector>
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
#include <boost/locale.hpp>

using boost::asio::ip::tcp;
using boost::system::error_code;
using boost::asio::io_context;
using boost::asio::ip::address;
using std::cin;
using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::vector;
using std::thread;

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
        boost::log::keywords::file_name = "./logs/sync_tcp_client.log", // 日志文件名
        boost::log::keywords::rotation_size = 10 * 1024 * 1024, // 日志文件大小
        boost::log::keywords::time_based_rotation = boost::log::sinks::file::rotation_at_time_point(0, 0, 0), // 日志文件时间
        boost::log::keywords::format = "[%TimeStamp% %ThreadID% %Severity%]: %Message%" // 日志格式
    );

    boost::log::add_console_log(std::cout, boost::log::keywords::format = "[%TimeStamp% %ThreadID% %Severity%]: %Message%");

    boost::log::add_common_attributes();

    return 0;
}

void sync_tcp_client_thread(const int ip_version)
{
    BOOST_LOG_TRIVIAL(info) << "ipv" << ip_version << "_thread" << " start.";

    io_context io;
    tcp::socket socket(io);

    tcp::endpoint end_point;
    switch (ip_version)
    {
    case 4:
    {
        end_point = tcp::endpoint(address::from_string("127.0.0.1"), 20712);
        break;
    }
    case 6:
    {
        end_point = tcp::endpoint(address::from_string("::1"), 20712);
        break;
    }
    default: // 默认选择ipv4
    {
        BOOST_LOG_TRIVIAL(warning) << "invalid ip version: " << ip_version << ", use ipv4";
        end_point = tcp::endpoint(address::from_string("127.0.0.1"), 20712);
        break;
    }
    }

    while (true)
    {
        boost::log::core::get()->flush(); // 刷新日志缓冲区

        error_code ec;

        // 连接服务器
        if (!socket.is_open())
        {
            socket.connect(end_point, ec);
            if (ec)
            {
                BOOST_LOG_TRIVIAL(error) << "ipv" << ip_version << "_thread" << " connect error: " << boost::locale::conv::between(ec.message(), "UTF-8", "GBK");
                socket.close();
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }
            BOOST_LOG_TRIVIAL(info) << "ipv" << ip_version << "_thread" << " connect success.";
        }

        // 接收数据
        vector<char> receive(1024);
        size_t receive_length = socket.read_some(boost::asio::buffer(receive), ec);
        if (ec == boost::asio::error::eof)
        {
            BOOST_LOG_TRIVIAL(info) << "ipv" << ip_version << "_thread" << " connection closed.";
            socket.close();
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }
        else if (ec)
        {
            BOOST_LOG_TRIVIAL(error) << "ipv" << ip_version << "_thread" << " read error: " << boost::locale::conv::between(ec.message(), "UTF-8", "GBK");

            // 断开连接
            socket.close();
            BOOST_LOG_TRIVIAL(info) << "ipv" << ip_version << "_thread" << " connection closed.";
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }
        BOOST_LOG_TRIVIAL(info) << "ipv" << ip_version << "_thread" << "receive: " << string(receive.data(), receive_length);
    }

    return;
}

// 同步tcp客户端测试
int main()
{
    int ret = 0;
    ret = init_log();
    if (ret != 0)
    {
        cerr << "init log failed" << endl;
        return -1;
    }

    BOOST_LOG_TRIVIAL(info) << "tcp sync client start.";

    thread thread_v4(sync_tcp_client_thread, 4);
    thread thread_v6(sync_tcp_client_thread, 6);

    thread_v4.join();
    thread_v6.join();

    return 0;
}
