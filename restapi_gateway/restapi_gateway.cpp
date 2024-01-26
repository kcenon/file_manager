/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2021, 🍀☀🌕🌥 🌊
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

#include <iostream>

#include "argument_parser.h"
#include "compressing.h"
#include "converting.h"
#include "file_handler.h"
#include "logging.h"
#include "messaging_client.h"
#include "messaging_server.h"

#include "container.h"
#include "value.h"
#include "values/bool_value.h"
#include "values/container_value.h"
#include "values/string_value.h"
#include "values/ushort_value.h"

#ifdef _CONSOLE
#include <Windows.h>
#endif

#include <future>
#include <signal.h>
#include <vector>

#include "fmt/format.h"
#include "fmt/xchar.h"

#include "cpprest/http_listener.h"
#include "cpprest/json.h"

constexpr auto PROGRAM_NAME = L"restapi_gateway";

using namespace std;
using namespace logging;
using namespace network;
using namespace converting;
using namespace compressing;
using namespace file_handler;
using namespace argument_parser;
using namespace container;

using namespace web;
using namespace web::http;
using namespace web::http::experimental::listener;

#ifdef _DEBUG
bool encrypt_mode = false;
bool compress_mode = false;
logging_level log_level = logging_level::parameter;
logging_styles logging_style = logging_styles::console_only;
#else
bool encrypt_mode = true;
bool compress_mode = true;
logging_level log_level = logging_level::information;
logging_styles logging_style = logging_styles::file_only;
#endif
unsigned short compress_block_size = 1024;

wstring connection_key = L"middle_connection_key";
wstring server_ip = L"127.0.0.1";
unsigned short server_port = 8642;
unsigned short rest_port = 7654;
unsigned short high_priority_count = 4;
unsigned short normal_priority_count = 4;
unsigned short low_priority_count = 4;

promise<bool> _promise_status;
future<bool> _future_status;

shared_ptr<messaging_client> _data_line = nullptr;
shared_ptr<http_listener> _http_listener = nullptr;

map<wstring, vector<shared_ptr<json::value>>> _messages;
map<wstring, function<void(shared_ptr<json::value>)>> _registered_restapi;
map<wstring, function<void(shared_ptr<container::value_container>)>>
    _registered_messages;

void signal_callback(int signum);

bool parse_arguments(argument_manager &arguments);

void create_data_line(void);
void create_http_listener(void);
void connection(const wstring &target_id, const wstring &target_sub_id,
                const bool &condition);

void received_message(shared_ptr<container::value_container> container);
void transfer_condition(shared_ptr<container::value_container> container);

void transfer_files(shared_ptr<json::value> request);

void get_method(http_request request);
void post_method(http_request request);

int main(int argc, char *argv[]) {
  argument_manager arguments(argc, argv);
  if (!parse_arguments(arguments)) {
    return 0;
  }

  signal(SIGINT, signal_callback);
  signal(SIGILL, signal_callback);
  signal(SIGABRT, signal_callback);
  signal(SIGFPE, signal_callback);
  signal(SIGSEGV, signal_callback);
  signal(SIGTERM, signal_callback);

  logger::handle().set_write_console(logging_style);
  logger::handle().set_target_level(log_level);
  logger::handle().start(PROGRAM_NAME);

  _registered_messages.insert({L"transfer_condition", transfer_condition});

  _registered_restapi.insert({L"upload_files", transfer_files});
  _registered_restapi.insert({L"download_files", transfer_files});

  create_data_line();
  create_http_listener();

  logger::handle().stop();

  return 0;
}

void signal_callback(int signum) {
  _promise_status.set_value(true);
  _http_listener.reset();

  _data_line.reset();

  logger::handle().stop();
}

bool parse_arguments(argument_manager &arguments) {
  auto bool_target = arguments.to_bool(L"--encrypt_mode");
  if (bool_target != nullopt) {
    encrypt_mode = *bool_target;
  }

  bool_target = arguments.to_bool(L"--compress_mode");
  if (bool_target != nullopt) {
    compress_mode = *bool_target;
  }

  auto ushort_target = arguments.to_ushort(L"--compress_block_size");
  if (ushort_target != nullopt) {
    compress_block_size = *ushort_target;
  }

  auto string_target = arguments.to_string(L"--server_ip");
  if (string_target != nullopt) {
    server_ip = *string_target;
  }

  ushort_target = arguments.to_ushort(L"--server_port");
  if (ushort_target != nullopt) {
    server_port = *ushort_target;
  }

  ushort_target = arguments.to_ushort(L"--rest_port");
  if (ushort_target != nullopt) {
    rest_port = *ushort_target;
  }

  ushort_target = arguments.to_ushort(L"--high_priority_count");
  if (ushort_target != nullopt) {
    high_priority_count = *ushort_target;
  }

  ushort_target = arguments.to_ushort(L"--normal_priority_count");
  if (ushort_target != nullopt) {
    normal_priority_count = *ushort_target;
  }

  ushort_target = arguments.to_ushort(L"--low_priority_count");
  if (ushort_target != nullopt) {
    low_priority_count = *ushort_target;
  }

  auto int_target = arguments.to_int(L"--logging_level");
  if (int_target != nullopt) {
    log_level = (logging_level)*int_target;
  }

  bool_target = arguments.to_bool(L"--write_console_only");
  if (bool_target != nullopt && *bool_target) {
    logging_style = logging_styles::console_only;

    return true;
  }

  bool_target = arguments.to_bool(L"--write_console");
  if (bool_target != nullopt && *bool_target) {
    logging_style = logging_styles::file_and_console;

    return true;
  }

  logging_style = logging_styles::file_only;

  return true;
}

void create_data_line(void) {
  if (_data_line != nullptr) {
    _data_line.reset();
  }

  _data_line = make_shared<messaging_client>(L"data_line");
  _data_line->set_compress_mode(compress_mode);
  _data_line->set_connection_key(connection_key);
  _data_line->set_session_types(session_types::message_line);
  _data_line->set_connection_notification(&connection);
  _data_line->set_message_notification(&received_message);
  _data_line->start(server_ip, server_port, high_priority_count,
                    normal_priority_count, low_priority_count);
}

void create_http_listener(void) {
#ifdef _WIN32
  _http_listener = make_shared<http_listener>(
      fmt::format(L"http://localhost:{}/restapi", rest_port));
#else
  _http_listener = make_shared<http_listener>(
      fmt::format("http://localhost:{}/restapi", rest_port));
#endif
  _http_listener->support(methods::GET, get_method);
  _http_listener->support(methods::POST, post_method);
  _http_listener->open()
      .then([&]() {
        logger::handle().write(logging_level::information,
                               L"starting to listen");
      })
      .wait();

  _future_status = _promise_status.get_future();
  _future_status.wait();
}

void connection(const wstring &target_id, const wstring &target_sub_id,
                const bool &condition) {
  if (_data_line == nullptr) {
    return;
  }

  logger::handle().write(
      logging_level::sequence,
      fmt::format(L"{} on middle server is {} from target: {}[{}]",
                  _data_line->source_id(),
                  condition ? L"connected" : L"disconnected", target_id,
                  target_sub_id));

  if (condition) {
    return;
  }

  this_thread::sleep_for(chrono::seconds(1));

  _data_line->start(server_ip, server_port, high_priority_count,
                    normal_priority_count, low_priority_count);
}

void received_message(shared_ptr<container::value_container> container) {
  if (container == nullptr) {
    return;
  }

  auto message_type = _registered_messages.find(container->message_type());
  if (message_type != _registered_messages.end()) {
    message_type->second(container);

    return;
  }

  logger::handle().write(
      logging_level::sequence,
      fmt::format(L"unknown message: {}", container->serialize()));
}

void transfer_condition(shared_ptr<container::value_container> container) {
  if (container == nullptr) {
    return;
  }

  if (container->message_type() != L"transfer_condition") {
    return;
  }

  wstring indication_id = L"";
  shared_ptr<json::value> condition =
      make_shared<json::value>(json::value::object(true));

  indication_id = container->get_value(L"indication_id")->to_string();

#ifdef _WIN32
  (*condition)[L"message_type"] =
      json::value::string(container->message_type());
  (*condition)[L"indication_id"] = json::value::string(indication_id);
  (*condition)[L"percentage"] =
      json::value::number(container->get_value(L"percentage")->to_ushort());
  (*condition)[L"completed"] =
      json::value::boolean(container->get_value(L"completed")->to_boolean());
#else
  (*condition)["message_type"] =
      json::value::string(converter::to_string(container->message_type()));
  (*condition)["indication_id"] =
      json::value::string(converter::to_string(indication_id));
  (*condition)["percentage"] =
      json::value::number(container->get_value(L"percentage")->to_ushort());
  (*condition)["completed"] =
      json::value::boolean(container->get_value(L"completed")->to_boolean());
#endif

  auto indication = _messages.find(indication_id);
  if (indication == _messages.end()) {
    _messages.insert({indication_id, {condition}});
    return;
  }

  indication->second.push_back(condition);
}

void transfer_files(shared_ptr<json::value> request) {
#ifdef _WIN32
  auto &file_array = (*request)[FILES].as_array();
#else
  auto &file_array = (*request)[converter::to_string(FILES)].as_array();
#endif

  vector<shared_ptr<container::value>> files;

#ifdef _WIN32
  files.push_back(make_shared<container::string_value>(
      L"indication_id", (*request)[L"indication_id"].as_string()));
#else
  files.push_back(make_shared<container::string_value>(
      L"indication_id",
      converter::to_wstring((*request)["indication_id"].as_string())));
#endif

  for (auto &file : file_array) {
    files.push_back(make_shared<container::container_value>(
        L"file", vector<shared_ptr<container::value>>{
#ifdef _WIN32
                     make_shared<container::string_value>(
                         L"source", file[SOURCE].as_string()),
                     make_shared<container::string_value>(
                         L"target", file[TARGET].as_string())
#else
                     make_shared<container::string_value>(
                         L"source",
                         converter::to_wstring(
                             file[converter::to_string(SOURCE)].as_string())),
                     make_shared<container::string_value>(
                         L"target",
                         converter::to_wstring(
                             file[converter::to_string(TARGET)].as_string()))
#endif
                 }));
  }

  shared_ptr<container::value_container> container =
#ifdef _WIN32
      make_shared<container::value_container>(
          L"main_server", L"", (*request)[MESSAGE_TYPE].as_string(), files);
#else
      make_shared<container::value_container>(
          L"main_server", L"",
          converter::to_wstring(
              (*request)[converter::to_string(MESSAGE_TYPE)].as_string()),
          files);
#endif

  _data_line->send(container);
}

void get_method(http_request request) {
  if (request.headers().empty()) {
    request.reply(status_codes::NotAcceptable);
    return;
  }

#ifdef _WIN32
  auto indication = _messages.find(request.headers()[INDICATION_ID]);
#else
  auto indication = _messages.find(converter::to_wstring(
      request.headers()[converter::to_string(INDICATION_ID)]));
#endif
  if (indication == _messages.end()) {
    request.reply(status_codes::NotAcceptable);
    return;
  }

  // do something
  vector<shared_ptr<json::value>> messages;

#ifdef _WIN32
  if (request.headers()[L"previous_message"] == L"clear")
#else
  if (request.headers()["previous_message"] == "clear")
#endif
  {
    messages.swap(indication->second);
  } else {
    messages = indication->second;
  }

  if (messages.empty()) {
    request.reply(status_codes::NoContent);
    return;
  }

  json::value answer = json::value::object(true);

#ifdef _WIN32
  answer[L"messages"] = json::value::array();
#else
  answer["messages"] = json::value::array();
#endif

  int index = 0;
  for (auto &message : messages) {
#ifdef _WIN32
    answer[L"messages"][index][MESSAGE_TYPE] = (*message)[MESSAGE_TYPE];
    answer[L"messages"][index][INDICATION_ID] = (*message)[INDICATION_ID];
    answer[L"messages"][index][L"percentage"] = (*message)[L"percentage"];
    answer[L"messages"][index][L"completed"] = (*message)[L"completed"];
#else
    answer["messages"][index][converter::to_string(MESSAGE_TYPE)] =
        (*message)[converter::to_string(MESSAGE_TYPE)];
    answer["messages"][index][converter::to_string(INDICATION_ID)] =
        (*message)[converter::to_string(INDICATION_ID)];
    answer["messages"][index]["percentage"] = (*message)["percentage"];
    answer["messages"][index]["completed"] = (*message)["completed"];
#endif

    index++;
  }

  request.reply(status_codes::OK, answer);
}

void post_method(http_request request) {
  auto action = request.extract_json().get();
  if (action.is_null()) {
    request.reply(status_codes::NoContent);
    return;
  }

#ifdef _WIN32
  logger::handle().write(logging_level::packet,
                         fmt::format(L"post method: {}", action.serialize()));
#else
  logger::handle().write(logging_level::packet,
                         converter::to_wstring(fmt::format(
                             "post method: {}", action.serialize())));
#endif

#ifdef _WIN32
  auto message_type =
      _registered_restapi.find(action[MESSAGE_TYPE].as_string());
#else
  auto message_type = _registered_restapi.find(converter::to_wstring(
      action[converter::to_string(MESSAGE_TYPE)].as_string()));
#endif
  if (message_type != _registered_restapi.end()) {
    message_type->second(make_shared<json::value>(action));

    request.reply(status_codes::OK);
    return;
  }

  request.reply(status_codes::NotImplemented);
}