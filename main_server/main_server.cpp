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

#include "logging.h"
#include "messaging_server.h"
#include "converting.h"
#include "compressing.h"
#include "file_handler.h"
#include "file_manager.h"
#include "argument_parser.h"

#include "value.h"
#include "values/ushort_value.h"
#include "values/string_value.h"

#include <wchar.h>
#include <algorithm>
#include <signal.h>

#ifdef _CONSOLE
#include <Windows.h>
#endif

#include "fmt/xchar.h"
#include "fmt/format.h"

constexpr auto PROGRAM_NAME = L"main_server";

using namespace std;
using namespace logging;
using namespace network;
using namespace converting;
using namespace compressing;
using namespace file_handler;
using namespace argument_parser;

#ifdef _DEBUG
bool write_console = true;
#else
bool write_console = false;
#endif
bool write_console_only = false;
bool encrypt_mode = false;
bool compress_mode = true;
unsigned short compress_block_size = 1024;
#ifdef _DEBUG
logging_level log_level = logging_level::packet;
#else
logging_level log_level = logging_level::information;
#endif
wstring connection_key = L"main_connection_key";
unsigned short server_port = 9753;
unsigned short high_priority_count = 4;
unsigned short normal_priority_count = 4;
unsigned short low_priority_count = 4;
size_t session_limit_count = 0;

shared_ptr<file_manager> _file_manager = nullptr;
shared_ptr<messaging_server> _main_server = nullptr;

#ifdef _CONSOLE
BOOL ctrl_handler(DWORD ctrl_type);
#endif

map<wstring, function<void(shared_ptr<container::value_container>)>> _registered_messages;

void parse_bool(const wstring& key, argument_manager& arguments, bool& value);
void parse_ushort(const wstring& key, argument_manager& arguments, unsigned short& value);
#ifdef _WIN32
void parse_ullong(const wstring& key, argument_manager& arguments, unsigned long long& value);
#else
void parse_ulong(const wstring& key, argument_manager& arguments, unsigned long& value);
#endif
bool parse_arguments(argument_manager& arguments);
void create_main_server(void);
void connection(const wstring& target_id, const wstring& target_sub_id, const bool& condition);

void received_message(shared_ptr<container::value_container> container);
void transfer_file(shared_ptr<container::value_container> container);
void upload_files(shared_ptr<container::value_container> container);

void received_file(const wstring& source_id, const wstring& source_sub_id, const wstring& indication_id, const wstring& target_path);
void display_help(void);

int main(int argc, char* argv[])
{
	argument_manager arguments(argc, argv);
	if (!parse_arguments(arguments))
	{
		return 0;
	}

#ifdef _CONSOLE
	SetConsoleCtrlHandler((PHANDLER_ROUTINE)ctrl_handler, TRUE);
#endif

	logger::handle().set_write_console(write_console, write_console_only);
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

#ifdef _CONSOLE
BOOL ctrl_handler(DWORD ctrl_type)
{
	switch (ctrl_type)
	{
	case CTRL_C_EVENT:
	case CTRL_CLOSE_EVENT:
	case CTRL_LOGOFF_EVENT:
	case CTRL_SHUTDOWN_EVENT:
	case CTRL_BREAK_EVENT:
		_main_server.reset();

		logger::handle().stop();
		break;
	}

	return FALSE;
}
#endif

void parse_bool(const wstring& key, argument_manager& arguments, bool& value)
{
	auto target = arguments.get(key);
	if (target.empty())
	{
		return;
	}

	auto temp = target;
	transform(temp.begin(), temp.end(), temp.begin(), ::tolower);

	value = temp.compare(L"true") == 0;
}

void parse_ushort(const wstring& key, argument_manager& arguments, unsigned short& value)
{
	auto target = arguments.get(key);
	if (!target.empty())
	{
		value = (unsigned short)atoi(converter::to_string(target).c_str());
	}
}

#ifdef _WIN32
void parse_ullong(const wstring& key, argument_manager& arguments, unsigned long long& value)
#else
void parse_ulong(const wstring& key, argument_manager& arguments, unsigned long& value)
#endif
{
	auto target = arguments.get(key);
	if (!target.empty())
	{
#ifdef _WIN32
		value = (unsigned long long)atoll(converter::to_string(target).c_str());
#else
		value = (unsigned long)atol(converter::to_string(target).c_str());
#endif
	}
}

bool parse_arguments(argument_manager& arguments)
{
	wstring temp;

	auto target = arguments.get(L"--help");
	if (!target.empty())
	{
		display_help();

		return false;
	}

	parse_bool(L"--encrypt_mode", arguments, encrypt_mode);
	parse_bool(L"--compress_mode", arguments, compress_mode);
	parse_ushort(L"--compress_block_size", arguments, compress_block_size);

	target = arguments.get(L"--connection_key");
	if (!target.empty())
	{
		temp = converter::to_wstring(file::load(target));
		if (!temp.empty())
		{
			connection_key = temp;
		}
	}
	
	parse_ushort(L"--server_port", arguments, server_port);
	parse_ushort(L"--high_priority_count", arguments, high_priority_count);
	parse_ushort(L"--normal_priority_count", arguments, normal_priority_count);
	parse_ushort(L"--low_priority_count", arguments, low_priority_count);
#ifdef _WIN32
	parse_ullong(L"--session_limit_count", arguments, session_limit_count);
#else
	parse_ulong(L"--session_limit_count", arguments, session_limit_count);
#endif
	parse_bool(L"--write_console", arguments, write_console);
	parse_bool(L"--write_console_only", arguments, write_console_only);

	target = arguments.get(L"--logging_level");
	if (!target.empty())
	{
		log_level = (logging_level)atoi(converter::to_string(target).c_str());
	}

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
	_main_server->set_possible_session_types({ session_types::message_line, session_types::file_line });
	_main_server->set_connection_notification(&connection);
	_main_server->set_message_notification(&received_message);
	_main_server->set_file_notification(&received_file);
	_main_server->start(server_port, high_priority_count, normal_priority_count, low_priority_count);
}

void connection(const wstring& target_id, const wstring& target_sub_id, const bool& condition)
{
	logger::handle().write(logging_level::information,
		fmt::format(L"a client on main server: {}[{}] is {}", target_id, target_sub_id, condition ? L"connected" : L"disconnected"));
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

	logger::handle().write(logging_level::information,
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

	logger::handle().write(logging_level::information, L"received message: transfer_file");

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

	vector<shared_ptr<container::value>> files = container->value_array(L"file");
	for (auto& file : files)
	{
		target_paths.push_back((*file)[L"target"]->to_string());
	}

	_file_manager->set(container->get_value(L"indication_id")->to_string(),
		container->get_value(L"gateway_source_id")->to_string(),
		container->get_value(L"gateway_source_sub_id")->to_string(), target_paths);

	if (_main_server)
	{
		_main_server->send(make_shared<container::value_container>(
			container->get_value(L"gatway_source_id")->to_string(), 
			container->get_value(L"gatway_source_sub_id")->to_string(),
			L"transfer_condition", 
			vector<shared_ptr<container::value>> {
				make_shared<container::string_value>(L"indication_id", container->get_value(L"indication_id")->to_string()),
				make_shared<container::ushort_value>(L"percentage", 0)
		}), session_types::file_line);
	}

	shared_ptr<container::value_container> temp = container->copy();
	temp->swap_header();

	temp->set_message_type(L"request_files");

	if (_main_server)
	{
		_main_server->send(temp, session_types::file_line);
	}
}

void received_file(const wstring& target_id, const wstring& target_sub_id, const wstring& indication_id, const wstring& target_path)
{
	logger::handle().write(logging_level::parameter,
		fmt::format(L"target_id: {}, target_sub_id: {}, indication_id: {}, file_path: {}", target_id, target_sub_id, indication_id, target_path));

	shared_ptr<container::value_container> container = _file_manager->received(indication_id, target_path);

	if (container != nullptr)
	{
		if (_main_server)
		{
			_main_server->send(container, session_types::file_line);
		}
	}
}

void display_help(void)
{
	wcout << L"Options:" << endl << endl;
	wcout << L"--encrypt_mode [value] " << endl;
	wcout << L"\tThe encrypt_mode on/off. If you want to use encrypt mode must be appended '--encrypt_mode true'.\n\tInitialize value is --encrypt_mode off." << endl << endl;
	wcout << L"--compress_mode [value]" << endl;
	wcout << L"\tThe compress_mode on/off. If you want to use compress mode must be appended '--compress_mode true'.\n\tInitialize value is --compress_mode off." << endl << endl;
	wcout << L"--compress_block_size [value]" << endl;
	wcout << L"\tThe compress_mode on/off. If you want to change compress block size must be appended '--compress_block_size size'.\n\tInitialize value is --compress_mode 1024." << endl << endl;
	wcout << L"--connection_key [value]" << endl;
	wcout << L"\tIf you want to change a specific key string for the connection to the main server must be appended\n\t'--connection_key [specific key string]'." << endl << endl;
	wcout << L"--server_port [value]" << endl;
	wcout << L"\tIf you want to change a port number for the connection to the main server must be appended\n\t'--server_port [port number]'." << endl << endl;
	wcout << L"--high_priority_count [value]" << endl;
	wcout << L"\tIf you want to change high priority thread workers must be appended '--high_priority_count [count]'." << endl << endl;
	wcout << L"--normal_priority_count [value]" << endl;
	wcout << L"\tIf you want to change normal priority thread workers must be appended '--normal_priority_count [count]'." << endl << endl;
	wcout << L"--low_priority_count [value]" << endl;
	wcout << L"\tIf you want to change low priority thread workers must be appended '--low_priority_count [count]'." << endl << endl;
	wcout << L"--session_limit_count [value]" << endl;
	wcout << L"\tIf you want to change session limit count must be appended '--session_limit_count [count]'." << endl << endl;
	wcout << L"--write_console_mode [value] " << endl;
	wcout << L"\tThe write_console_mode on/off. If you want to display log on console must be appended '--write_console_mode true'.\n\tInitialize value is --write_console_mode off." << endl << endl;
	wcout << L"--logging_level [value]" << endl;
	wcout << L"\tIf you want to change log level must be appended '--logging_level [level]'." << endl;
}
