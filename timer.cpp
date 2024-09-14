#include <iostream>
#include <boost/asio.hpp>

void on_timer_expired(const boost::system::error_code& e)
{
    if (!e)
    {
        std::cout << "Timer expired!" << std::endl;
    }
}

// 定时器测试
int main()
{
    boost::asio::io_context io;
    boost::asio::steady_timer timer(io, boost::asio::chrono::seconds(20));

    timer.async_wait(&on_timer_expired);
    io.run();

    return 0;
}
