#pragma once
#include <echo_session.h>

class EchoServer {
          tcp::acceptor acc_;

public:
          EchoServer(io_context& ioc, uint16_t port);

private:
          void do_accept();
};
