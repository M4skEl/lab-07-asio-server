#include <algorithm>
#include <chrono>
#include <ctime>
#include <header.hpp>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "boost/asio.hpp"
#include "boost/log/trivial.hpp"
#include "boost/thread/pthread/recursive_mutex.hpp"
#include "boost/thread/thread.hpp"
using boost::asio::io_service;
using boost::asio::ip::tcp;

static io_service service;
boost::recursive_mutex mtx;

class Talk_to_client {
  std::chrono::system_clock::time_point log_time;
  std::string name;
  tcp::socket socket_;

 public:
  Talk_to_client()
      : log_time(std::chrono::system_clock::now()), socket_(service) {}

  std::string user_name() { return name; }

  void SetName(const std::string& nick) { name = nick; }

  bool time_is_over() {
    return (std::chrono::system_clock::now() - log_time).count() > 5;
  }

  void answer_to_client(std::string message) {
    socket_.write_some(boost::asio::buffer(message));
  }

  tcp::socket& socket() { return socket_; }
};

std::vector<std::shared_ptr<Talk_to_client>> clients;

std::string get_all_users() {
  std::string names;
  for (auto& client : clients) names += client->user_name() + " ";
  return names;
}

void accept_threads() {
  tcp::acceptor acceptor(service, tcp::endpoint(tcp::v4(), 8001));
  while (true) {
    BOOST_LOG_TRIVIAL(info)
        << "start logging \n Thread id: " << std::this_thread::get_id() << "\n";
    auto new_client = std::make_shared<Talk_to_client>();
    acceptor.accept(new_client->socket());
    boost::recursive_mutex::scoped_lock lk(mtx);
    std::string nickname;
    new_client->socket().read_some(boost::asio::buffer(nickname, 25));
    new_client->SetName(nickname);
    clients.push_back(new_client);
    new_client->answer_to_client("You are login to server");
    BOOST_LOG_TRIVIAL(info) << "Login OK \n user_name: " << nickname << "\n";
  }
}

void handle_clients_thread() {
  while (true) {
    boost::this_thread::sleep(boost::posix_time::millisec(1));
    boost::recursive_mutex::scoped_lock lk(mtx);
    for (auto& client : clients) {
      std::string request;
      client->socket().read_some(boost::asio::buffer(request, 100));
      if (request == "client list") {
        client->answer_to_client(get_all_users());
        BOOST_LOG_TRIVIAL(info)
            << "Answer to client username: " << client->user_name() << "/n";
      } else if (request == "ping") {
        client->answer_to_client("Ping OK");
        BOOST_LOG_TRIVIAL(info)
            << "Answer to client username: " << client->user_name() << "/n";
      }
    }
    clients.erase(std::remove_if(clients.begin(), clients.end(),
                                 [](std::shared_ptr<Talk_to_client> ptr) {
                                   return ptr->time_is_over();
                                 }),
                  clients.end());
  }
}

int main() {
  boost::thread_group threads;
  threads.create_thread(accept_threads);
  threads.create_thread(handle_clients_thread);
  threads.join_all();
  return 0;
}