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

#include "container.h"
#include "network/network.h"

#include <algorithm>
#include <signal.h>
#include <wchar.h>
#include <locale>

#ifdef _CONSOLE
#include <Windows.h>
#endif

#include "fmt/format.h"
#include "fmt/xchar.h"

#include "file_manager.h"

constexpr auto PROGRAM_NAME = "main_server";

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
bool write_console = true;
#else
bool encrypt_mode = true;
bool compress_mode = true;
log_types log_level = log_types::Information;
bool write_console = false;
#endif
unsigned short compress_block_size = 1024;

string connection_key = "main_connection_key";
unsigned short server_port = 9753;
unsigned short high_priority_count = 4;
unsigned short normal_priority_count = 4;
unsigned short low_priority_count = 4;
size_t session_limit_count = 0;

shared_ptr<file_manager> _file_manager = nullptr;
shared_ptr<messaging_server> _main_server = nullptr;

void signal_callback(int signum);

map<string, function<void(shared_ptr<value_container>)>>
	_registered_messages;

bool parse_arguments(argument_manager& arguments);
void create_main_server(void);
void connection(const wstring& target_id,
				const wstring& target_sub_id,
				const bool& condition);

void received_message(shared_ptr<value_container> container);
void transfer_file(shared_ptr<value_container> container);
void upload_files(shared_ptr<value_container> container);

void received_file(const wstring& source_id,
				   const wstring& source_sub_id,
				   const wstring& indication_id,
				   const wstring& target_path);

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

	log_module::set_title(PROGRAM_NAME);
	if (write_console) {
		log_module::console_target(log_level);
	}
	log_module::file_target(log_level);
	log_module::start();

	_registered_messages.insert({ "transfer_file", &transfer_file });
	_registered_messages.insert({ "upload_files", &upload_files });

	_file_manager = make_shared<file_manager>();

	create_main_server();

	// Keep the server running until signal is received
	while (_main_server != nullptr) {
		this_thread::sleep_for(chrono::milliseconds(100));
	}

	log_module::stop();

	return 0;
}

void signal_callback(int signum) { 
	log_module::write_information(						   fmt::format("Received signal {}. Shutting down server...", signum).c_str());
	if (_main_server) {
		_main_server->stop_server();
		_main_server.reset();
	}
}

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

	ushort_target = arguments.to_ushort("--server_port");
	if (ushort_target != std::nullopt)
	{
		server_port = *ushort_target;
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
		write_console = true;

		return true;
	}

	bool_target = arguments.to_bool("--write_console");
	if (bool_target != std::nullopt && *bool_target)
	{
		write_console = true;

		return true;
	}

	write_console = false;

	return true;
}

void create_main_server(void)
{
	if (_main_server != nullptr)
	{
		_main_server.reset();
	}

	// Create messaging_server with program name
	_main_server = make_shared<messaging_server>(PROGRAM_NAME);
	
	// Start the server (the new API has simplified initialization)
	_main_server->start_server(server_port);
	
	log_module::write_information(
				fmt::format("Main server started on port {}", server_port).c_str());
}

void connection(const wstring& target_id,
				const wstring& target_sub_id,
				const bool& condition)
{
	auto [id_str, id_err] = convert_string::to_string(target_id);
	auto [sub_id_str, sub_id_err] = convert_string::to_string(target_sub_id);
	log_module::write_information(
				fmt::format("a client on main server: {}[{}] is {}", 
					id_str.value_or(""), sub_id_str.value_or(""),
					condition ? "connected" : "disconnected").c_str());
}

void received_message(shared_ptr<value_container> container)
{
	if (container == nullptr)
	{
		return;
	}

	string message_type = container->message_type();
	auto message_handler = _registered_messages.find(message_type);
	if (message_handler != _registered_messages.end())
	{
		message_handler->second(container);
		return;
	}

	string serialized = container->serialize();
	log_module::write_information(
				fmt::format("received message: {}", serialized).c_str());
}

void transfer_file(shared_ptr<value_container> container)
{
	if (container == nullptr)
	{
		return;
	}

	if (container->message_type() != "transfer_file")
	{
		return;
	}

	log_module::write_information(						   "received message: transfer_file");

	// Note: The new messaging_server API doesn't have send_files method
	// This would need to be implemented differently or the messaging system 
	// would need to be extended to support file transfers
	log_module::write_information(						   "File transfer not implemented in new messaging system API");
}

void upload_files(shared_ptr<value_container> container)
{
	if (container == nullptr)
	{
		return;
	}

	if (container->message_type() != "upload_files")
	{
		return;
	}

	log_module::write_information(						   "received message: upload_files");

	// Note: The new container API is different from the old value_container
	// This would need to be reimplemented using the new container API
	// which uses set_value/get_value methods with string keys
	
	try {
		// Extract indication_id if present
		auto indication_value = container->get_value("indication_id");
		if (indication_value) {
			string indication_id = indication_value->to_string();
			log_module::write_information(								   fmt::format("Processing upload for indication_id: {}", 
											  indication_id).c_str());
		}
		
		log_module::write_information(							   "Upload files processing needs to be reimplemented for new API");
	} catch (const exception& e) {
		string error_msg = e.what();
		log_module::write_information(							   fmt::format("Error processing upload_files: {}", 
										  error_msg).c_str());
	}
}

void received_file(const wstring& target_id,
				   const wstring& target_sub_id,
				   const wstring& indication_id,
				   const wstring& target_path)
{
	auto [tid_str, tid_err] = convert_string::to_string(target_id);
	auto [tsid_str, tsid_err] = convert_string::to_string(target_sub_id);
	auto [iid_str, iid_err] = convert_string::to_string(indication_id);
	auto [tp_str, tp_err] = convert_string::to_string(target_path);
	log_module::write_information(						   fmt::format("target_id: {}, target_sub_id: {}, "
									   "indication_id: {}, file_path: {}",
									   tid_str.value_or(""), tsid_str.value_or(""), 
									   iid_str.value_or(""), tp_str.value_or("")).c_str());

	// Note: file_manager->received returns old API container
	// This would need to be updated to work with the new messaging system
	log_module::write_information(						   "File reception handling needs to be reimplemented for new API");
}
