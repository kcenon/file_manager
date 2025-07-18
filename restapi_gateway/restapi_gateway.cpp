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

// Using httplib and nlohmann_json instead of cpprestsdk
#include <httplib.h>
#include <nlohmann/json.hpp>

constexpr auto PROGRAM_NAME = L"restapi_gateway";

using namespace std;
using namespace logging;
using namespace network;
using namespace converting;
using namespace compressing;
using namespace file_handler;
using namespace argument_parser;
using namespace container;

// Use nlohmann::json instead of web::json
using json = nlohmann::json;

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
shared_ptr<httplib::Server> _http_server = nullptr;  // Using httplib::Server

map<wstring, vector<shared_ptr<json>>> _messages;
map<wstring, function<void(shared_ptr<json>)>> _registered_restapi;
map<wstring, function<void(shared_ptr<container::value_container>)>>
	_registered_messages;

void signal_callback(int signum);

bool parse_arguments(argument_manager& arguments);

void create_data_line(void);
void create_http_server(void);
void connection(const wstring& target_id,
				const wstring& target_sub_id,
				const bool& condition);

void received_message(shared_ptr<container::value_container> container);
void transfer_condition(shared_ptr<container::value_container> container);

void transfer_files(shared_ptr<json> request);

int main(int argc, char* argv[])
{
	argument_manager arguments(argc, argv);
	if (!parse_arguments(arguments))
	{
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

	_registered_messages.insert({ L"transfer_condition", transfer_condition });

	_registered_restapi.insert({ L"upload_files", transfer_files });
	_registered_restapi.insert({ L"download_files", transfer_files });

	create_data_line();
	create_http_server();

	logger::handle().stop();

	return 0;
}

void signal_callback(int signum)
{
	_promise_status.set_value(true);
	if (_http_server) {
		_http_server->stop();  // Stop httplib server
	}

	_data_line.reset();

	logger::handle().stop();
}

bool parse_arguments(argument_manager& arguments)
{
	auto bool_target = arguments.to_bool(L"--encrypt_mode");
	if (bool_target != nullopt)
	{
		encrypt_mode = *bool_target;
	}

	bool_target = arguments.to_bool(L"--compress_mode");
	if (bool_target != nullopt)
	{
		compress_mode = *bool_target;
	}

	auto ushort_target = arguments.to_ushort(L"--compress_block_size");
	if (ushort_target != nullopt)
	{
		compress_block_size = *ushort_target;
	}

	auto string_target = arguments.to_string(L"--server_ip");
	if (string_target != nullopt)
	{
		server_ip = *string_target;
	}

	ushort_target = arguments.to_ushort(L"--server_port");
	if (ushort_target != nullopt)
	{
		server_port = *ushort_target;
	}

	ushort_target = arguments.to_ushort(L"--rest_port");
	if (ushort_target != nullopt)
	{
		rest_port = *ushort_target;
	}

	ushort_target = arguments.to_ushort(L"--high_priority_count");
	if (ushort_target != nullopt)
	{
		high_priority_count = *ushort_target;
	}

	ushort_target = arguments.to_ushort(L"--normal_priority_count");
	if (ushort_target != nullopt)
	{
		normal_priority_count = *ushort_target;
	}

	ushort_target = arguments.to_ushort(L"--low_priority_count");
	if (ushort_target != nullopt)
	{
		low_priority_count = *ushort_target;
	}

	auto int_target = arguments.to_int(L"--logging_level");
	if (int_target != nullopt)
	{
		log_level = (logging_level)*int_target;
	}

	bool_target = arguments.to_bool(L"--write_console_only");
	if (bool_target != nullopt && *bool_target)
	{
		logging_style = logging_styles::console_only;

		return true;
	}

	bool_target = arguments.to_bool(L"--write_console");
	if (bool_target != nullopt && *bool_target)
	{
		logging_style = logging_styles::file_and_console;

		return true;
	}

	logging_style = logging_styles::file_only;

	return true;
}

void create_data_line(void)
{
	if (_data_line != nullptr)
	{
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

void create_http_server(void)
{
	// Create httplib server
	_http_server = make_shared<httplib::Server>();

	// Handle GET requests
	_http_server->Get("/restapi", [](const httplib::Request& req, httplib::Response& res) {
		if (req.headers.empty()) {
			res.status = 406; // Not Acceptable
			return;
		}

		auto indication_id_it = req.headers.find("indication_id");
		if (indication_id_it == req.headers.end()) {
			res.status = 406; // Not Acceptable
			return;
		}

		// Convert to wstring for map lookup
		auto indication = _messages.find(converter::to_wstring(indication_id_it->second));
		if (indication == _messages.end()) {
			res.status = 406; // Not Acceptable
			return;
		}

		// Do something
		vector<shared_ptr<json>> messages;

		auto prev_msg_it = req.headers.find("previous_message");
		if (prev_msg_it != req.headers.end() && prev_msg_it->second == "clear") {
			messages.swap(indication->second);
		} else {
			messages = indication->second;
		}

		if (messages.empty()) {
			res.status = 204; // No Content
			return;
		}

		// Build JSON response
		json answer;
		answer["messages"] = json::array();

		int index = 0;
		for (auto& message : messages) {
			answer["messages"][index] = *message;
			index++;
		}

		res.set_content(answer.dump(), "application/json");
	});

	// Handle POST requests
	_http_server->Post("/restapi", [](const httplib::Request& req, httplib::Response& res) {
		if (req.body.empty()) {
			res.status = 204; // No Content
			return;
		}

		try {
			// Parse JSON from request body
			json action = json::parse(req.body);
			
			logger::handle().write(
				logging_level::packet,
				converter::to_wstring(fmt::format("post method: {}", action.dump())));
			
			auto message_type_it = action.find("message_type");
			if (message_type_it != action.end()) {
				std::string message_type = message_type_it->get<std::string>();
				
				// Find handler for this message type
				auto message_handler = _registered_restapi.find(converter::to_wstring(message_type));
				if (message_handler != _registered_restapi.end()) {
					auto json_ptr = make_shared<json>(action);
					message_handler->second(json_ptr);
					
					res.status = 200; // OK
					return;
				}
			}
			
			res.status = 501; // Not Implemented
			
		} catch (const std::exception& e) {
			logger::handle().write(
				logging_level::error,
				converter::to_wstring(fmt::format("JSON parse error: {}", e.what())));
			res.status = 400; // Bad Request
		}
	});

	// Start server
	logger::handle().write(logging_level::information, L"starting to listen");
	_http_server->listen("localhost", rest_port);
	
	_future_status = _promise_status.get_future();
	_future_status.wait();
}

void connection(const wstring& target_id,
				const wstring& target_sub_id,
				const bool& condition)
{
	if (_data_line == nullptr)
	{
		return;
	}

	logger::handle().write(
		logging_level::sequence,
		fmt::format(L"{} on middle server is {} from target: {}[{}]",
					_data_line->source_id(),
					condition ? L"connected" : L"disconnected", target_id,
					target_sub_id));

	if (condition)
	{
		return;
	}

	this_thread::sleep_for(chrono::seconds(1));

	_data_line->start(server_ip, server_port, high_priority_count,
					  normal_priority_count, low_priority_count);
}

void received_message(shared_ptr<container::value_container> container)
{
	if (container == nullptr)
	{
		return;
	}

	auto message_type = _registered_messages.find(container->message_type());
	if (message_type != _registered_messages.end())
	{
		message_type->second(container);

		return;
	}

	logger::handle().write(
		logging_level::sequence,
		fmt::format(L"unknown message: {}", container->serialize()));
}

void transfer_condition(shared_ptr<container::value_container> container)
{
	if (container == nullptr)
	{
		return;
	}

	if (container->message_type() != L"transfer_condition")
	{
		return;
	}

	wstring indication_id = L"";
	shared_ptr<json> condition = make_shared<json>();

	indication_id = container->get_value(L"indication_id")->to_string();

	// Fill the JSON with values
	(*condition)["message_type"] = converter::to_string(container->message_type());
	(*condition)["indication_id"] = converter::to_string(indication_id);
	(*condition)["percentage"] = container->get_value(L"percentage")->to_ushort();
	(*condition)["completed"] = container->get_value(L"completed")->to_boolean();

	auto indication = _messages.find(indication_id);
	if (indication == _messages.end())
	{
		_messages.insert({ indication_id, { condition } });
		return;
	}

	indication->second.push_back(condition);
}

void transfer_files(shared_ptr<json> request)
{
	if (!request || !request->contains("files") || !request->at("files").is_array()) {
		return;
	}

	auto& file_array = request->at("files");

	vector<shared_ptr<container::value>> files;

	files.push_back(make_shared<container::string_value>(
		L"indication_id",
		converter::to_wstring(request->at("indication_id").get<std::string>())));

	for (auto& file : file_array) {
		files.push_back(make_shared<container::container_value>(
			L"file", vector<shared_ptr<container::value>>{
						 make_shared<container::string_value>(
							 L"source", converter::to_wstring(file["source"].get<std::string>())),
						 make_shared<container::string_value>(
							 L"target", converter::to_wstring(file["target"].get<std::string>()))
					 }));
	}

	shared_ptr<container::value_container> container =
		make_shared<container::value_container>(
			L"main_server", L"",
			converter::to_wstring(request->at("message_type").get<std::string>()),
			files);

	_data_line->send(container);
}