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
#include "messaging_client.h"
#include "messaging_server.h"

#include "value.h"
#include "values/bool_value.h"
#include "values/string_value.h"
#include "values/ushort_value.h"

#ifdef _CONSOLE
#include <Windows.h>
#endif

#include <signal.h>

#include "fmt/format.h"
#include "fmt/xchar.h"

constexpr auto PROGRAM_NAME = L"middle_server";

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

wstring main_connection_key = L"main_connection_key";
wstring middle_connection_key = L"middle_connection_key";
unsigned short middle_server_port = 8642;
wstring main_server_ip = L"127.0.0.1";
unsigned short main_server_port = 9753;
unsigned short high_priority_count = 4;
unsigned short normal_priority_count = 4;
unsigned short low_priority_count = 4;
size_t session_limit_count = 0;

map<wstring, function<void(shared_ptr<container::value_container>)>>
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
	shared_ptr<container::value_container> container);
void received_message_from_file_line(
	shared_ptr<container::value_container> container);

void received_file_from_file_line(const wstring& source_id,
								  const wstring& source_sub_id,
								  const wstring& indication_id,
								  const wstring& target_path);

void download_files(shared_ptr<container::value_container> container);
void upload_files(shared_ptr<container::value_container> container);
void uploaded_file(shared_ptr<container::value_container> container);

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

	_file_commands.insert({ L"download_files", &download_files });
	_file_commands.insert({ L"upload_files", &upload_files });

	logger::handle().set_write_console(logging_style);
	logger::handle().set_target_level(log_level);
	logger::handle().start(PROGRAM_NAME);

	_file_manager = make_shared<file_manager>();

	create_middle_server();
	create_file_line();

	_middle_server->wait_stop();

	_file_line->stop();

	logger::handle().stop();

	return 0;
}

void signal_callback(int signum) { _middle_server.reset(); }

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

	auto string_target = arguments.to_string(L"--main_server_ip");
	if (string_target != nullopt)
	{
		main_server_ip = *string_target;
	}

	ushort_target = arguments.to_ushort(L"--main_server_port");
	if (ushort_target != nullopt)
	{
		main_server_port = *ushort_target;
	}

	ushort_target = arguments.to_ushort(L"--middle_server_port");
	if (ushort_target != nullopt)
	{
		middle_server_port = *ushort_target;
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

void create_middle_server(void)
{
	if (_middle_server != nullptr)
	{
		_middle_server.reset();
	}

	_middle_server = make_shared<messaging_server>(PROGRAM_NAME);
	_middle_server->set_encrypt_mode(encrypt_mode);
	_middle_server->set_compress_mode(compress_mode);
	_middle_server->set_connection_key(middle_connection_key);
	_middle_server->set_session_limit_count(session_limit_count);
	_middle_server->set_possible_session_types({ session_types::message_line });
	_middle_server->set_connection_notification(&connection_from_middle_server);
	_middle_server->set_message_notification(
		&received_message_from_middle_server);
	_middle_server->start(middle_server_port, high_priority_count,
						  normal_priority_count, low_priority_count);
}

void create_file_line(void)
{
	if (_file_line != nullptr)
	{
		_file_line.reset();
	}

	_file_line = make_shared<messaging_client>(L"file_line");
	_file_line->set_bridge_line(true);
	_file_line->set_compress_mode(compress_mode);
	_file_line->set_connection_key(main_connection_key);
	_file_line->set_session_types(session_types::file_line);
	_file_line->set_connection_notification(&connection_from_file_line);
	_file_line->set_message_notification(&received_message_from_file_line);
	_file_line->set_file_notification(&received_file_from_file_line);
	_file_line->start(main_server_ip, main_server_port, high_priority_count,
					  normal_priority_count, low_priority_count);
}

void connection_from_middle_server(const wstring& target_id,
								   const wstring& target_sub_id,
								   const bool& condition)
{
	logger::handle().write(
		logging_level::information,
		fmt::format(L"a client on middle server: {}[{}] is {}", target_id,
					target_sub_id, condition ? L"connected" : L"disconnected"));
}

void received_message_from_middle_server(
	shared_ptr<container::value_container> container)
{
	if (container == nullptr)
	{
		return;
	}

	if (_file_line == nullptr
		|| _file_line->get_confirm_status() != connection_conditions::confirmed)
	{
		if (_middle_server == nullptr)
		{
			return;
		}

		shared_ptr<container::value_container> response
			= container->copy(false);
		response->swap_header();

		response << make_shared<container::bool_value>(L"error", true);
		response << make_shared<container::string_value>(
			L"reason", L"main_server has not been connected.");

		_middle_server->send(response);

		return;
	}

	auto target = _file_commands.find(container->message_type());
	if (target == _file_commands.end())
	{
		shared_ptr<container::value_container> response
			= container->copy(false);
		response->swap_header();

		response << make_shared<container::bool_value>(L"error", true);
		response << make_shared<container::string_value>(
			L"reason", L"cannot parse unknown message");

		_middle_server->send(response);

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

	logger::handle().write(
		logging_level::sequence,
		fmt::format(L"{} on middle server is {} from target: {}[{}]",
					_file_line->source_id(),
					condition ? L"connected" : L"disconnected", target_id,
					target_sub_id));

	if (condition)
	{
		return;
	}

	if (_middle_server == nullptr)
	{
		return;
	}

	this_thread::sleep_for(chrono::seconds(1));

	_file_line->start(main_server_ip, main_server_port, high_priority_count,
					  normal_priority_count, low_priority_count);
}

void received_message_from_file_line(
	shared_ptr<container::value_container> container)
{
	if (container == nullptr)
	{
		return;
	}

	if (container->message_type() == L"uploaded_file")
	{
		uploaded_file(container);

		return;
	}

	if (_middle_server)
	{
		_middle_server->send(container);
	}
}

void received_file_from_file_line(const wstring& target_id,
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
		if (_middle_server)
		{
			_middle_server->send(container);
		}
	}
}

void download_files(shared_ptr<container::value_container> container)
{
	if (container == nullptr)
	{
		return;
	}

	if (_file_line == nullptr
		|| _file_line->get_confirm_status() != connection_conditions::confirmed)
	{
		if (_middle_server == nullptr)
		{
			return;
		}

		shared_ptr<container::value_container> response
			= container->copy(false);
		response->swap_header();

		response << make_shared<container::bool_value>(L"error", true);
		response << make_shared<container::string_value>(
			L"reason", L"main_server has not been connected.");

		_middle_server->send(response);

		return;
	}

	logger::handle().write(
		logging_level::information,
		L"attempt to prepare downloading files from main_server");

	vector<wstring> target_paths;
	vector<shared_ptr<container::value>> files
		= container->value_array(L"file");
	if (files.empty())
	{
		shared_ptr<container::value_container> response
			= container->copy(false);
		response->swap_header();

		response << make_shared<container::bool_value>(L"error", true);
		response << make_shared<container::string_value>(
			L"reason",
			L"cannot download with empty file information (source or "
			L"target) from main_server.");

		_middle_server->send(response);

		return;
	}

	logger::handle().write(logging_level::information, container->serialize());

	for (auto& file : files)
	{
		logger::handle().write(logging_level::information, file->serialize());

		auto data_array = file->value_array(L"target");
		if (data_array.empty())
		{
			continue;
		}

		target_paths.push_back(data_array[0]->to_string());
	}

	if (target_paths.empty())
	{
		shared_ptr<container::value_container> response
			= container->copy(false);
		response->swap_header();

		response << make_shared<container::bool_value>(L"error", true);
		response << make_shared<container::string_value>(
			L"reason",
			L"cannot download with empty target file information from "
			L"main_server.");

		_middle_server->send(response);

		return;
	}

	_file_manager->set(container->get_value(L"indication_id")->to_string(),
					   container->source_id(), container->source_sub_id(),
					   target_paths);

	logger::handle().write(
		logging_level::information,
		L"prepared parsing of downloading files from main_server");

	if (_middle_server)
	{
		_middle_server->send(make_shared<container::value_container>(
			container->source_id(), container->source_sub_id(),
			L"transfer_condition",
			vector<shared_ptr<container::value>>{
				make_shared<container::string_value>(
					L"indication_id",
					container->get_value(L"indication_id")->to_string()),
				make_shared<container::ushort_value>(L"percentage", 0) }));
	}

	shared_ptr<container::value_container> temp = container->copy();
	temp->set_message_type(L"request_files");

	if (_file_line)
	{
		_file_line->send(temp);
	}
}

void upload_files(shared_ptr<container::value_container> container)
{
	if (container == nullptr)
	{
		return;
	}

	if (_file_line == nullptr
		|| _file_line->get_confirm_status() != connection_conditions::confirmed)
	{
		if (_middle_server == nullptr)
		{
			return;
		}

		shared_ptr<container::value_container> response
			= container->copy(false);
		response->swap_header();

		response << make_shared<container::bool_value>(L"error", true);
		response << make_shared<container::string_value>(
			L"reason", L"main_server has not been connected.");

		_middle_server->send(response);

		return;
	}

	logger::handle().write(
		logging_level::information,
		L"attempt to prepare uploading files to main_server");

	if (_file_line)
	{
		container << make_shared<container::string_value>(
			L"gateway_source_id", container->source_id());
		container << make_shared<container::string_value>(
			L"gateway_source_sub_id", container->source_sub_id());
		container->set_source(_file_line->source_id(),
							  _file_line->source_sub_id());

		_file_line->send(container);
	}
}

void uploaded_file(shared_ptr<container::value_container> container)
{
	if (container == nullptr)
	{
		return;
	}

	shared_ptr<container::value_container> temp = _file_manager->received(
		container->get_value(L"indication_id")->to_string(),
		container->get_value(L"target_path")->to_string());

	if (temp != nullptr)
	{
		if (_middle_server)
		{
			_middle_server->send(temp);
		}
	}
}