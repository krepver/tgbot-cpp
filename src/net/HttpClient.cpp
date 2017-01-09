/*
 * Copyright (c) 2015 Oleg Morozenkov
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "tgbot/net/HttpClient.h"

#include <boost/asio/ssl.hpp>

using namespace std;
using namespace boost::asio;
using namespace boost::asio::ip;

namespace TgBot {

HttpClient& HttpClient::getInstance() {
  static HttpClient result;
  return result;
}

string HttpClient::makeRequest(const Url& url, const vector<HttpReqArg>& args) {
  ssl::context context(ssl::context::sslv23);
  context.set_default_verify_paths();

  ssl::stream<tcp::socket> socket(_ioService, context);
  tcp::resolver resolver(_ioService);
  tcp::resolver::query query(url.host, url.protocol);

  connect(socket.lowest_layer(), resolver.resolve(query));

  socket.set_verify_mode(ssl::verify_none);
  socket.set_verify_callback(ssl::rfc2818_verification(url.host));
  socket.handshake(ssl::stream<tcp::socket>::client);

  string requestText = HttpParser::getInstance().generateRequest(url, args, false);
  write(socket, buffer(requestText.c_str(), requestText.length()));

  string response;
  char buff[1024];
  boost::system::error_code error;
  while (!error) {
    size_t bytes = read(socket, buffer(buff), error);
    response += string(buff, bytes);
  }

  return HttpParser::getInstance().parseResponse(response);
}

void HttpClient::makeAsyncRequest(const Url& url, const vector<HttpReqArg>& args) {
  ssl::context context(ssl::context::sslv23);
  context.set_default_verify_paths();

  std::shared_ptr<ssl::stream<tcp::socket> > socket( new ssl::stream<tcp::socket>(*_extService, context));
  std::shared_ptr<tcp::resolver> resolver(new tcp::resolver(*_extService));
  tcp::resolver::query query(url.host, url.protocol);

  resolver->async_resolve(query, [socket, resolver, url, args](const boost::system::error_code& error,
                          tcp::resolver::iterator iterator){
    if (error) {
//      cout << "async_resolve: " << error.value() << " - " << error.message() << endl;
      return;
    }
    async_connect(socket->lowest_layer(), iterator, [socket, url, args](const boost::system::error_code& er,
                  tcp::resolver::iterator){
      if (er) {
//        cout << "async_connect: " << er.value() << " - " << er.message() << endl;
        return;
      }
//      cout << "async_connect: ok" << endl;
      socket->set_verify_mode(ssl::verify_none);
      socket->set_verify_callback(ssl::rfc2818_verification(url.host));
      socket->async_handshake(ssl::stream<tcp::socket>::client, [socket, url, args](const boost::system::error_code& err){
        if (err) {
//          cout << "async_handshake: " << err.value() << " - " << err.message() << endl;
          return;
        }
//        cout << "async_handshake: ok" << endl;
        string requestText = HttpParser::getInstance().generateRequest(url, args, false);
        async_write(*socket, buffer(requestText.c_str(), requestText.length()), [socket](const boost::system::error_code& ec,
                    std::size_t){
          if (ec) {
//            cout << "async_write: " << ec.value() << " - " << ec.message() << endl;
            return;
          }
//          cout << "async_write: ok" << endl;
//          boost::shared_ptr<char[]> buff(new char[1024]);
          char* buff = new char[1024];
          socket->async_read_some(buffer(buff, 1024), [buff, socket](const boost::system::error_code& e,
                     std::size_t ){
            delete[] buff;
            if (e) {
//              cout << "async_read: " << e.value() << " - " << e.message() << endl;
              return;
            }
//            cout << "async_read: ok" << endl;
            socket->async_shutdown([socket](const boost::system::error_code& erro){
              if (erro) {
//                if ((erro.category() == boost::asio::error::get_ssl_category())
//                  && (ERR_GET_REASON(erro.value()) == SSL_R_SHORT_READ)) {
//                  cout << "async_shutdown ssl error: " << ERR_GET_REASON(erro.value()) << " - " << erro.message() << endl;
//                } else {
//                  cout << "async_shutdown: " << erro.value() << " - " << erro.message() << endl;
//                }
                return;
              }
//              cout << "async_shutdown: ok" << endl;
            });
          });
        });
      });
    });
  });
}

}
