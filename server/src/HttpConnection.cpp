#include<ada.h>
#include<http/HttpConnection.hpp>
#include<handler/HandleMethod.hpp>

HTTPConnection::HTTPConnection(boost::asio::ip::tcp::socket &_socket)
          :http_socket(_socket)
{
}

void HTTPConnection::start_service()
{
          activate_receiver();
          check_timeout();
}

void HTTPConnection::check_timeout()
{
          /*extended HTTPConnection class life time*/
          std::shared_ptr<HTTPConnection> extended_lifetime = shared_from_this();

          http_timer.async_wait([extended_lifetime, this](boost::system::error_code ec) {
                    if (!ec) {
                              /*if timeout then shutdown connection*/
                              extended_lifetime->http_socket.close();
                    }
          });
}

void HTTPConnection::activate_receiver()
{
          /*extended HTTPConnection class life time*/
          std::shared_ptr<HTTPConnection> extended_lifetime = shared_from_this();

          boost::beast::http::async_read(http_socket, http_buffer, http_request,
                    [this, extended_lifetime](boost::system::error_code ec, std::size_t bytes_transferred) {
                              boost::ignore_unused(bytes_transferred);
                              if (!ec) {          /*no error occured!*/
                                        process_request();
                              }
                    }
          );
}

void HTTPConnection::process_request()
{
          /*short connection*/
          http_response.keep_alive(false);
          http_response.version(http_request.version());

          /*extended HTTPConnection class life time*/
          std::shared_ptr<HTTPConnection> extended_lifetime = shared_from_this();

          /*HTTP GET method*/
          switch (http_request.method())
          {
          case  boost::beast::http::verb::get:
                    handle_get_request(extended_lifetime);
                    write_response();
                    break;

          case boost::beast::http::verb::post:
                    break;

          default:
                    return_not_found();
                    write_response();
                    break;
          }
}

void HTTPConnection::write_response()
{
          /*Set Http Body length*/
          http_response.content_length(http_response.body().size());

          /*extended HTTPConnection class life time*/
          std::shared_ptr<HTTPConnection> extended_lifetime = shared_from_this();

          boost::beast::http::async_write(http_socket, http_response,
                    [this, extended_lifetime](boost::beast::error_code ec, std::size_t bytes_transferred) {
                              extended_lifetime->http_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);

                              /*because http has already been sent, so cancel timer*/
                              extended_lifetime->http_timer.cancel();
                    }
          );
}

void HTTPConnection::handle_get_request(std::shared_ptr<HTTPConnection> extended_lifetime)
{
          /*store url info /path?username=me&password=passwd*/
          this->http_url_info = http_request.target();

          std::size_t pos = this->http_url_info.find_first_of('?');
          std::string_view url_path = this->http_url_info.substr(0, pos);
          std::string_view url_param = this->http_url_info.substr(pos + 1);

#ifdef _DEBUG
          std::cerr << "url_path = " << url_path << '\n'
                        << "url_param = " << url_param << '\n';
#endif // _DEBUG

          ada::url_search_params parameters(url_param);
          for (const auto& param : parameters) {
                    this->http_params.emplace(param.first, param.second);
          }

          /*fix bug: because url_path is a std::string_view so when using .data method it will return all the data inside http_url_info*/
          if (!HandleMethod::get_instance()->handleGetMethod(std::string(url_path), extended_lifetime)){
                    return_not_found();
          }
          else
          {
                    http_response.result(boost::beast::http::status::ok);
                    http_response.set(boost::beast::http::field::server, "Beast GateServer");
          }
}

void HTTPConnection::return_not_found()
{
          http_response.result(boost::beast::http::status::not_found);         //Set Response Status
          http_response.set(boost::beast::http::field::content_type, "text/plain");
          boost::beast::ostream(http_response.body()) << "404 Not Found!";
}