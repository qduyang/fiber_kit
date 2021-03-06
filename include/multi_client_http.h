#ifndef MULTI_CLIENT_HTTP_HPP
#define MULTI_CLIENT_HTTP_HPP

#include <string>
#include <thread>
#include <list>
#include <map>
#include <memory>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/fiber/all.hpp>
using std::string;
using std::list;
using std::map;

#include "web_utility.hpp"
#include "exception_trace.hpp"

using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
using namespace boost::beast;    // from <boost/beast/http.hpp>
using boost::posix_time::ptime;
namespace ssl = boost::asio::ssl;

typedef boost::asio::ip::tcp::resolver::results_type ResolverResult;
typedef boost::asio::ip::tcp::endpoint  Endpoint;
typedef boost::system::error_code BSError;

namespace nghttp2 {
namespace asio_http2 {
namespace client {
class session;
}
}
}

struct HttpConnection
{
    string host;
    string port;
    std::shared_ptr<tcp::socket> socket_ptr;
    int dns_timeout = 5;
    int conn_timeout = 3;
    int req_timeout = 6;
    bool in_use = false;
    ptime last_use;
};

typedef std::shared_ptr<HttpConnection> HttpConnectionPtr;

struct HttpsConnection
{
    string host;
    string port;
    std::shared_ptr<ssl::stream<tcp::socket>> stream_ptr;
    int dns_timeout = 5;
    int conn_timeout = 3;
    int handshake_timeout = 3;
    int req_timeout = 6;
    bool in_use = false;
    ptime last_use;
};

typedef std::shared_ptr<HttpsConnection> HttpsConnectionPtr;

struct Http2sConnection
{
    string host;
    string port;
    std::shared_ptr<nghttp2::asio_http2::client::session> session_ptr;
    ptime last_use;
};

struct Http2Resopnse
{
    int status;
    int body_size = 0;
    string body;
};

typedef http::request<http::string_body> StrRequest;
typedef http::response<http::string_body> StrResponse;

class MultiClientHttp
{
public:
    MultiClientHttp(int thread_count = 1);
    ~MultiClientHttp();


    //http请求
    string request(const string& host, const string& port, boost::beast::http::verb method, const string& target, const string& body) noexcept;

    StrResponse request(const string& host, const string& port, const StrRequest &req) noexcept;


    //https请求

    //需要确认证书
    string request(const string& host, const string& port, boost::asio::ssl::context::method ssl_method, const string& cert,
                   boost::beast::http::verb method, const string& target, const string& body)  noexcept;

    StrResponse request(const string& host, const string& port, boost::asio::ssl::context::method ssl_method, const string& cert, StrRequest &req) noexcept;

    //不用确认证书
    StrResponse request(const string& host, const string& port, boost::asio::ssl::context::method ssl_method, StrRequest &req) noexcept;

    //双向确认

    //http/2 ssl请求
    //不用确认证书
    Http2Resopnse request2(const string& host, const string& port, boost::asio::ssl::context::method ssl_method,
                          boost::beast::http::verb method, const string& target, const std::map<string,string>& headers, const string& body) noexcept;


    Http2Resopnse request2(const string& host, const string& port, boost::asio::ssl::context::method ssl_method, const string& cert,
                          boost::beast::http::verb method, const string& target, const std::map<string,string>& headers, const string& body) noexcept;

private:
    //http1.1请求缓存操作
    HttpConnectionPtr get_http_connect(const string& host, const string& port) noexcept;
    void release_http_connect(HttpConnectionPtr conn_ptr) noexcept;
    void delete_invalid_http_connect(HttpConnectionPtr conn_ptr) noexcept;
    void delete_timeout_http_connect() noexcept;

    //https 1.1请求缓存操作
    HttpsConnectionPtr get_https_connect(const string& host, const string& port, boost::asio::ssl::context::method ssl_method,
                                                                    const string& cert) noexcept;
    void release_https_connect(HttpsConnectionPtr stream_ptr) noexcept;
    void delete_invalid_https_connect(HttpsConnectionPtr stream_ptr) noexcept;
    void delete_timeout_https_connect() noexcept;

    //https 2 请求缓存操作
    std::shared_ptr<nghttp2::asio_http2::client::session> get_http2s_connect_stream(const string& host, const string& port, boost::asio::ssl::context::method ssl_method,
                                                                    const string& cert) noexcept;
    //void release_http2s_connect_stream(std::shared_ptr<nghttp2::asio_http2::client::session> session_ptr);
    void delete_invalid_http2s_connect(std::shared_ptr<nghttp2::asio_http2::client::session> session_ptr) noexcept;
    void delete_timeout_http2s_connect() noexcept;

private:
    bool resolve(const string& host, const string& port, int timeout, ResolverResult& rr) noexcept;
    bool connect(tcp::socket& socket, const ResolverResult& rr, int timeout) noexcept;
    bool set_ssl(ssl::context& ctx, const string& cert) noexcept;

protected:
    int m_thread_count = 1;
    boost::asio::io_context m_io_cxt;
    typedef boost::asio::executor_work_guard<boost::asio::io_context::executor_type> io_context_work;
    std::unique_ptr<io_context_work> m_work;
    std::vector<std::thread> m_threads;

    size_t m_timeout = 3000;
    size_t m_timeout_connect = 2000;

    boost::fibers::mutex m_http_mutex;
    list<HttpConnectionPtr> m_cache_http_conns;

    boost::fibers::mutex m_https_mutex;
    list<HttpsConnectionPtr> m_cache_https_streams;

    boost::fibers::mutex m_http2s_mutex;
    list<Http2sConnection> m_cache_http2s_session;

    int m_unuse_timeout = 55; //超过55秒没有使用就断掉

    boost::fibers::condition_variable_any m_stop_cnd;
    boost::fibers::mutex m_stop_mux;
    boost::fibers::fiber m_timer_fiber;
    bool m_running = true;
};

#endif /* MULTI_CLIENT_HTTP_HPP */
