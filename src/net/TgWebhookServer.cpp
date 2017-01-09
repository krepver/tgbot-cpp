#include "tgbot/net/TgWebhookServer.h"

#include "tgbot/net/HttpClient.h"

namespace TgBot {
TgWebhookServer::TgWebhookServer(const std::string& publicKey,
                                        const std::string& privateKey,
                                        std::shared_ptr<boost::asio::io_service>& service,
                                        int port,
                                        const std::string& path,
                                        const EventHandler* eventHandler):
  _server(publicKey, privateKey),
  _handler(eventHandler)
{
  HttpClient::getInstance()._extService = service;
  _server.config.thread_pool_size = 0;
  _server.config.port=port;
  _server.io_service = service;

  //Add resources using path-regex and method-string, and an anonymous function
  //POST-example for the path /path, responds the posted string
  _server.resource["^/"+ path +"$"]["POST"]=[this](std::shared_ptr<HttpsServer::Response> response, std::shared_ptr<HttpsServer::Request> request) {
    //Retrieve string:
    auto data = request->content.string();
//    std::cout << data << std::endl;
    _handler->handleUpdate(TgTypeParser::getInstance().parseJsonAndGetUpdate(TgTypeParser::getInstance().parseJson(data)));
//    _handler->handleUpdate(TgTypeParser::getInstance().parseJsonAndGetUpdate(TgTypeParser::getInstance().parseJsonRapid(data)));

    //request->content.string() is a convenience function for:
    //stringstream ss;
    //ss << request->content.rdbuf();
    //string content=ss.str();

    *response << "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
  };

  _server.start();
}

void TgWebhookServer::stop()
{
  _server.stop();
}

}
