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
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <functional>

#include "utilities/parsing/argument_parser.h"
#include "utilities/conversion/convert_string.h"
#include "utilities/io/file_handler.h"
#include "logger/core/logger.h"
#include "network/network.h"

#include "container/container.h"
#include "values/bool_value.h"
#include "values/string_value.h"
#include "values/numeric_value.h"

#ifdef _CONSOLE
#include <Windows.h>
#endif

#include <signal.h>

#include "fmt/format.h"
#include "fmt/xchar.h"

#include "file_manager.h"

constexpr auto PROGRAM_NAME = "middle_server";

using namespace std;
using namespace log_module;
using namespace network_module;
using namespace container_module;
using namespace utility_module;
// file_handler는 utility_module에 포함됨
// argument_parser는 utility_module에 포함됨

#ifdef _DEBUG
bool encrypt_mode = false;
bool compress_mode = false;
log_types log_level = log_types::Parameter;
bool logging_style = true;
#else
bool encrypt_mode = true;
bool compress_mode = true;
log_types log_level = log_types::Information;
bool logging_style = false;
#endif
unsigned short compress_block_size = 1024;

wstring main_connection_key = L"main_connection_key";
wstring middle_connection_key = L"middle_connection_key";
unsigned short middle_server_port = 8642;
wstring main_server_ip = L"127.0.0.1";
unsigned short main_server_port = 9753;
unsigned short high_priority_count = 4;
unsigned short normal_priority_count = 4;
unsigned short low_priority_count = 4;
size_t session_limit_count = 0;

map<string, function<void(shared_ptr<value_container>)>>
	_file_commands;

shared_ptr<file_manager> _file_manager = nullptr;
shared_ptr<messaging_client> _file_line = nullptr;
shared_ptr<messaging_server> _middle_server = nullptr;

void signal_callback(int signum);

bool parse_arguments(argument_manager& arguments);

void create_middle_server(void);
void create_file_line(void);
void connection_from_middle_server(const wstring& target_id,
								   const wstring& target_sub_id,
								   const bool& condition);
void connection_from_file_line(const wstring& target_id,
							   const wstring& target_sub_id,
							   const bool& condition);

void received_message_from_middle_server(
	shared_ptr<value_container> container);
void received_message_from_file_line(
	shared_ptr<value_container> container);

void received_file_from_file_line(const wstring& source_id,
								  const wstring& source_sub_id,
								  const wstring& indication_id,
								  const wstring& target_path);

void download_files(shared_ptr<value_container> container);
void upload_files(shared_ptr<value_container> container);
void uploaded_file(shared_ptr<value_container> container);

int main(int argc, char* argv[])
{
	argument_manager arguments;
	auto result = arguments.try_parse(argc, argv);
	if (result.has_value()) {
		std::wcout << "Argument parsing failed: " << std::wstring(result.value().begin(), result.value().end()) << std::endl;
		return 0;
	}
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

	_file_commands.insert({ "download_files", &download_files });
	_file_commands.insert({ "upload_files", &upload_files });

	log_module::set_title(PROGRAM_NAME);
	if (logging_style) {
		log_module::console_target(log_level);
	}
	log_module::file_target(log_level);
	log_module::start();

	_file_manager = make_shared<file_manager>();

	create_middle_server();
	create_file_line();

	// Keep the server running until signaled
	while (_middle_server != nullptr) {
		this_thread::sleep_for(chrono::milliseconds(100));
	}

	_file_line->stop_client();

	log_module::stop();

	return 0;
}

void signal_callback(int signum) { _middle_server.reset(); }

bool parse_arguments(argument_manager& arguments)
{
	auto bool_target = arguments.to_bool("--encrypt_mode");
	if (bool_target != std::nullopt)
	{
		encrypt_mode = *bool_target;
	}

	bool_target = arguments.to_bool("--compress_mode");
	if (bool_target != std::nullopt)
	{
		compress_mode = *bool_target;
	}

	auto ushort_target = arguments.to_ushort("--compress_block_size");
	if (ushort_target != std::nullopt)
	{
		compress_block_size = *ushort_target;
	}

	auto string_target = arguments.to_string("--main_server_ip");
	if (string_target != std::nullopt)
	{
		auto [wide_str, err] = convert_string::to_wstring(*string_target);
		if (wide_str.has_value()) {
			main_server_ip = wide_str.value();
		}
	}

	ushort_target = arguments.to_ushort("--main_server_port");
	if (ushort_target != std::nullopt)
	{
		main_server_port = *ushort_target;
	}

	ushort_target = arguments.to_ushort("--middle_server_port");
	if (ushort_target != std::nullopt)
	{
		middle_server_port = *ushort_target;
	}

	ushort_target = arguments.to_ushort("--high_priority_count");
	if (ushort_target != std::nullopt)
	{
		high_priority_count = *ushort_target;
	}

	ushort_target = arguments.to_ushort("--normal_priority_count");
	if (ushort_target != std::nullopt)
	{
		normal_priority_count = *ushort_target;
	}

	ushort_target = arguments.to_ushort("--low_priority_count");
	if (ushort_target != std::nullopt)
	{
		low_priority_count = *ushort_target;
	}

	auto int_target = arguments.to_int("--log_types");
	if (int_target != std::nullopt)
	{
		log_level = (log_types)*int_target;
	}

#ifdef _WIN32
	auto ullong_target = arguments.to_ullong("--session_limit_count");
	if (ullong_target != std::nullopt)
	{
		session_limit_count = *ullong_target;
	}
#else
	auto long_target = arguments.to_long("--session_limit_count");
	if (long_target != std::nullopt && *long_target >= 0)
	{
		session_limit_count = static_cast<size_t>(*long_target);
	}
#endif

	bool_target = arguments.to_bool("--write_console_only");
	if (bool_target != std::nullopt && *bool_target)
	{
		logging_style = true;

		return true;
	}

	bool_target = arguments.to_bool("--write_console");
	if (bool_target != std::nullopt && *bool_target)
	{
		logging_style = true;

		return true;
	}

	logging_style = false;

	return true;
}

void create_middle_server(void)
{
	if (_middle_server != nullptr)
	{
		_middle_server.reset();
	}

	_middle_server = make_shared<messaging_server>(PROGRAM_NAME);
	// TODO: set_encrypt_mode, set_compress_mode, set_connection_key, set_session_limit_count, 
	// set_possible_session_types, set_connection_notification, set_message_notification 
	// APIs are not available in the new messaging_server
	// Need to implement these features or use alternative approach
	_middle_server->start_server(middle_server_port);
}

void create_file_line(void)
{
	if (_file_line != nullptr)
	{
		_file_line.reset();
	}

	_file_line = make_shared<messaging_client>("file_line");
	// TODO: set_bridge_line, set_compress_mode, set_connection_key, set_session_types,
	// set_connection_notification, set_message_notification, set_file_notification
	// APIs are not available in the new messaging_client
	// Need to implement these features or use alternative approach
	_file_line->start_client(std::get<0>(convert_string::to_string(main_server_ip)).value_or(""), main_server_port);
}

void connection_from_middle_server(const wstring& target_id,
								   const wstring& target_sub_id,
								   const bool& condition)
{
	auto [tid_str, tid_err] = convert_string::to_string(target_id);
	auto [tsid_str, tsid_err] = convert_string::to_string(target_sub_id);
	log_module::write_information(
		fmt::format("a client on middle server: {}[{}] is {}", tid_str.value_or(""),
					tsid_str.value_or(""), condition ? "connected" : "disconnected").c_str());
}

void received_message_from_middle_server(
	shared_ptr<value_container> container)
{
	if (container == nullptr)
	{
		return;
	}

	if (_file_line == nullptr
		// TODO: get_confirm_status() API is not available in new messaging_client
		/* || _file_line->get_confirm_status() != connection_conditions::confirmed */)
	{
		if (_middle_server == nullptr)
		{
			return;
		}

		shared_ptr<value_container> response
			= container->copy(false);
		response->swap_header();

		response << make_shared<bool_value>("error", true);
		response << make_shared<string_value>(
			"reason", "main_server has not been connected.");

		// TODO: _middle_server->send(response) API is not available
		// Need to implement alternative approach

		return;
	}

	auto target = _file_commands.find(container->message_type());
	if (target == _file_commands.end())
	{
		shared_ptr<value_container> response
			= container->copy(false);
		response->swap_header();

		response << make_shared<bool_value>("error", true);
		response << make_shared<string_value>(
			"reason", "cannot parse unknown message");

		// TODO: _middle_server->send(response) API is not available
		// Need to implement alternative approach

		return;
	}

	target->second(container);
}

void connection_from_file_line(const wstring& target_id,
							   const wstring& target_sub_id,
							   const bool& condition)
{
	if (_file_line == nullptr)
	{
		return;
	}

	auto [tid_str, tid_err] = convert_string::to_string(target_id);
	auto [tsid_str, tsid_err] = convert_string::to_string(target_sub_id);
	log_module::write_sequence(
		fmt::format("{} on middle server is {} from target: {}[{}]",
					"file_line",  // TODO: _file_line->source_id() not available
					condition ? "connected" : "disconnected", tid_str.value_or(""),
					tsid_str.value_or("")).c_str());

	if (condition)
	{
		return;
	}

	if (_middle_server == nullptr)
	{
		return;
	}

	this_thread::sleep_for(chrono::seconds(1));

	_file_line->start_client(std::get<0>(convert_string::to_string(main_server_ip)).value_or(""), main_server_port);
}

void received_message_from_file_line(
	shared_ptr<value_container> container)
{
	if (container == nullptr)
	{
		return;
	}

	if (container->message_type() == "uploaded_file")
	{
		uploaded_file(container);

		return;
	}

	if (_middle_server)
	{
		// TODO: _middle_server->send(container) API is not available
		// Need to implement alternative approach
	}
}

void received_file_from_file_line(const wstring& target_id,
								  const wstring& target_sub_id,
								  const wstring& indication_id,
								  const wstring& target_path)
{
	auto [tid_str, tid_err] = convert_string::to_string(target_id);
	auto [tsid_str, tsid_err] = convert_string::to_string(target_sub_id);
	auto [iid_str, iid_err] = convert_string::to_string(indication_id);
	auto [tp_str, tp_err] = convert_string::to_string(target_path);
	log_module::write_information(
		fmt::format("target_id: {}, target_sub_id: {}, "
					"indication_id: {}, file_path: {}",
					tid_str.value_or(""), tsid_str.value_or(""), iid_str.value_or(""),
					tp_str.value_or("")).c_str());

	shared_ptr<value_container> container
		= _file_manager->received(indication_id, target_path);

	if (container != nullptr)
	{
		if (_middle_server)
		{
			// TODO: _middle_server->send(container) API is not available
			// Need to implement alternative approach
		}
	}
}

void download_files(shared_ptr<value_container> container)
{
	if (container == nullptr)
	{
		return;
	}

	if (_file_line == nullptr
		// TODO: get_confirm_status() API is not available in new messaging_client
		/* || _file_line->get_confirm_status() != connection_conditions::confirmed */)
	{
		if (_middle_server == nullptr)
		{
			return;
		}

		shared_ptr<value_container> response
			= container->copy(false);
		response->swap_header();

		response << make_shared<bool_value>("error", true);
		response << make_shared<string_value>(
			"reason", "main_server has not been connected.");

		// TODO: _middle_server->send(response) API is not available
		// Need to implement alternative approach

		return;
	}

	log_module::write_information(
		"attempt to prepare downloading files from main_server");

	vector<wstring> target_paths;
	vector<shared_ptr<value>> files
		= container->value_array("file");
	if (files.empty())
	{
		shared_ptr<value_container> response
			= container->copy(false);
		response->swap_header();

		response << make_shared<bool_value>("error", true);
		response << make_shared<string_value>(
			"reason",
			"cannot download with empty file information (source or "
			"target) from main_server.");

		// TODO: _middle_server->send(response) API is not available
		// Need to implement alternative approach

		return;
	}

	log_module::write_information(container->serialize().c_str());

	for (auto& file : files)
	{
		log_module::write_information(file->serialize().c_str());

		auto data_array = file->value_array("target");
		if (data_array.empty())
		{
			continue;
		}

		auto [wide_str, err] = convert_string::to_wstring(data_array[0]->to_string());
		if (wide_str.has_value()) {
			target_paths.push_back(wide_str.value());
		}
	}

	if (target_paths.empty())
	{
		shared_ptr<value_container> response
			= container->copy(false);
		response->swap_header();

		response << make_shared<bool_value>("error", true);
		response << make_shared<string_value>(
			"reason",
			"cannot download with empty target file information from "
			"main_server.");

		// TODO: _middle_server->send(response) API is not available
		// Need to implement alternative approach

		return;
	}

	auto [iid_str, iid_err] = convert_string::to_wstring(container->get_value("indication_id")->to_string());
	auto [sid_str, sid_err] = convert_string::to_wstring(container->source_id());
	auto [ssid_str, ssid_err] = convert_string::to_wstring(container->source_sub_id());
	if (iid_str.has_value() && sid_str.has_value() && ssid_str.has_value()) {
		_file_manager->set(iid_str.value(), sid_str.value(), ssid_str.value(), target_paths);
	}

	log_module::write_information(
		"prepared parsing of downloading files from main_server");

	if (_middle_server)
	{
		// TODO: _middle_server->send API is not available
		// Need to implement alternative approach for sending transfer_condition message
		auto temp_container = make_shared<value_container>(
			container->source_id(), container->source_sub_id(),
			"transfer_condition",
			vector<shared_ptr<value>>{
				make_shared<string_value>(
					"indication_id",
					container->get_value("indication_id")->to_string()),
				make_shared<numeric_value<unsigned short, value_types::ushort_value>>("percentage", 0) });
		// _middle_server->send(temp_container);
	}

	shared_ptr<value_container> temp = container->copy();
	temp->set_message_type("request_files");

	if (_file_line)
	{
		// TODO: _file_line->send(temp) API is not available
		// Need to implement alternative approach
	}
}

void upload_files(shared_ptr<value_container> container)
{
	if (container == nullptr)
	{
		return;
	}

	if (_file_line == nullptr
		// TODO: get_confirm_status() API is not available in new messaging_client
		/* || _file_line->get_confirm_status() != connection_conditions::confirmed */)
	{
		if (_middle_server == nullptr)
		{
			return;
		}

		shared_ptr<value_container> response
			= container->copy(false);
		response->swap_header();

		response << make_shared<bool_value>("error", true);
		response << make_shared<string_value>(
			"reason", "main_server has not been connected.");

		// TODO: _middle_server->send(response) API is not available
		// Need to implement alternative approach

		return;
	}

	log_module::write_information(
		"attempt to prepare uploading files to main_server");

	if (_file_line)
	{
		// TODO: Container modification and sending through file_line
		// The new messaging_client API doesn't support these operations
		// Need to implement alternative approach for:
		// 1. Adding gateway_source_id and gateway_source_sub_id to container
		// 2. Setting source information
		// 3. Sending container through file_line
	}
}

void uploaded_file(shared_ptr<value_container> container)
{
	if (container == nullptr)
	{
		return;
	}

	auto [iid_str2, iid_err2] = convert_string::to_wstring(container->get_value("indication_id")->to_string());
	auto [tp_str2, tp_err2] = convert_string::to_wstring(container->get_value("target_path")->to_string());
	shared_ptr<value_container> temp;
	if (iid_str2.has_value() && tp_str2.has_value()) {
		temp = _file_manager->received(iid_str2.value(), tp_str2.value());
	}

	if (temp != nullptr)
	{
		if (_middle_server)
		{
			// TODO: _middle_server->send(temp) API is not available
			// Need to implement alternative approach
		}
	}
}