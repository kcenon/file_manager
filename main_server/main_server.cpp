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
#include "file_manager.h"
#include "logging.h"
#include "messaging_server.h"

#include "value.h"
#include "values/string_value.h"
#include "values/ushort_value.h"

#include <algorithm>
#include <signal.h>
#include <wchar.h>

#ifdef _CONSOLE
#include <Windows.h>
#endif

#include "fmt/format.h"
#include "fmt/xchar.h"

constexpr auto PROGRAM_NAME = L"main_server";

using namespace std;
using namespace logging;
using namespace network;
using namespace converting;
using namespace compressing;
using namespace file_handler;
using namespace argument_parser;

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

wstring connection_key = L"main_connection_key";
unsigned short server_port = 9753;
unsigned short high_priority_count = 4;
unsigned short normal_priority_count = 4;
unsigned short low_priority_count = 4;
size_t session_limit_count = 0;

shared_ptr<file_manager> _file_manager = nullptr;
shared_ptr<messaging_server> _main_server = nullptr;

void signal_callback(int signum);

map<wstring, function<void(shared_ptr<container::value_container>)>>
	_registered_messages;

bool parse_arguments(argument_manager& arguments);
void create_main_server(void);
void connection(const wstring& target_id,
				const wstring& target_sub_id,
				const bool& condition);

void received_message(shared_ptr<container::value_container> container);
void transfer_file(shared_ptr<container::value_container> container);
void upload_files(shared_ptr<container::value_container> container);

void received_file(const wstring& source_id,
				   const wstring& source_sub_id,
				   const wstring& indication_id,
				   const wstring& target_path);

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

	_registered_messages.insert({ L"transfer_file", &transfer_file });
	_registered_messages.insert({ L"upload_files", &upload_files });

	_file_manager = make_shared<file_manager>();

	create_main_server();

	_main_server->wait_stop();

	logger::handle().stop();

	return 0;
}

void signal_callback(int signum) { _main_server.reset(); }

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

	ushort_target = arguments.to_ushort(L"--server_port");
	if (ushort_target != nullopt)
	{
		server_port = *ushort_target;
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

#ifdef _WIN32
	auto ullong_target = arguments.to_ullong(L"--session_limit_count");
	if (ullong_target != nullopt)
	{
		session_limit_count = *ullong_target;
	}
#else
	auto ulong_target = arguments.to_ulong(L"--session_limit_count");
	if (ulong_target != nullopt)
	{
		session_limit_count = *ulong_target;
	}
#endif

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

void create_main_server(void)
{
	if (_main_server != nullptr)
	{
		_main_server.reset();
	}

	_main_server = make_shared<messaging_server>(PROGRAM_NAME);
	_main_server->set_encrypt_mode(encrypt_mode);
	_main_server->set_compress_mode(compress_mode);
	_main_server->set_connection_key(connection_key);
	_main_server->set_session_limit_count(session_limit_count);
	_main_server->set_possible_session_types(
		{ session_types::message_line, session_types::file_line });
	_main_server->set_connection_notification(&connection);
	_main_server->set_message_notification(&received_message);
	_main_server->set_file_notification(&received_file);
	_main_server->start(server_port, high_priority_count, normal_priority_count,
						low_priority_count);
}

void connection(const wstring& target_id,
				const wstring& target_sub_id,
				const bool& condition)
{
	logger::handle().write(
		logging_level::information,
		fmt::format(L"a client on main server: {}[{}] is {}", target_id,
					target_sub_id, condition ? L"connected" : L"disconnected"));
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
		logging_level::information,
		fmt::format(L"received message: {}", container->serialize()));
}

void transfer_file(shared_ptr<container::value_container> container)
{
	if (container == nullptr)
	{
		return;
	}

	if (container->message_type() != L"transfer_file")
	{
		return;
	}

	logger::handle().write(logging_level::information,
						   L"received message: transfer_file");

	if (_main_server != nullptr)
	{
		_main_server->send_files(container);
	}
}

void upload_files(shared_ptr<container::value_container> container)
{
	if (container == nullptr)
	{
		return;
	}

	if (container->message_type() != L"upload_files")
	{
		return;
	}

	vector<wstring> target_paths;

	vector<shared_ptr<container::value>> files
		= container->value_array(L"file");
	for (auto& file : files)
	{
		target_paths.push_back((*file)[L"target"]->to_string());
	}

	_file_manager->set(
		container->get_value(L"indication_id")->to_string(),
		container->get_value(L"gateway_source_id")->to_string(),
		container->get_value(L"gateway_source_sub_id")->to_string(),
		target_paths);

	if (_main_server)
	{
		_main_server->send(
			make_shared<container::value_container>(
				container->get_value(L"gatway_source_id")->to_string(),
				container->get_value(L"gatway_source_sub_id")->to_string(),
				L"transfer_condition",
				vector<shared_ptr<container::value>>{
					make_shared<container::string_value>(
						L"indication_id",
						container->get_value(L"indication_id")->to_string()),
					make_shared<container::ushort_value>(L"percentage", 0) }),
			session_types::file_line);
	}

	shared_ptr<container::value_container> temp = container->copy();
	temp->swap_header();

	temp->set_message_type(L"request_files");

	if (_main_server)
	{
		_main_server->send(temp, session_types::file_line);
	}
}

void received_file(const wstring& target_id,
				   const wstring& target_sub_id,
				   const wstring& indication_id,
				   const wstring& target_path)
{
	logger::handle().write(logging_level::parameter,
						   fmt::format(L"target_id: {}, target_sub_id: {}, "
									   L"indication_id: {}, file_path: {}",
									   target_id, target_sub_id, indication_id,
									   target_path));

	shared_ptr<container::value_container> container
		= _file_manager->received(indication_id, target_path);

	if (container != nullptr)
	{
		if (_main_server)
		{
			_main_server->send(container, session_types::file_line);
		}
	}
}
