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
#include "messaging_client.h"
#include "converting.h"
#include "compressing.h"
#include "file_handler.h"
#include "file_manager.h"
#include "argument_parser.h"

#include "value.h"
#include "values/bool_value.h"
#include "values/ushort_value.h"
#include "values/string_value.h"

#ifdef _CONSOLE
#include <Windows.h>
#endif

#include <signal.h>

#include "fmt/xchar.h"
#include "fmt/format.h"

constexpr auto PROGRAM_NAME = L"middle_server";

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
wstring main_connection_key = L"main_connection_key";
wstring middle_connection_key = L"middle_connection_key";
unsigned short middle_server_port = 8642;
wstring main_server_ip = L"127.0.0.1";
unsigned short main_server_port = 9753;
unsigned short high_priority_count = 4;
unsigned short normal_priority_count = 4;
unsigned short low_priority_count = 4;
size_t session_limit_count = 0;

map<wstring, function<void(shared_ptr<container::value_container>)>> _file_commands;

shared_ptr<file_manager> _file_manager = nullptr;
shared_ptr<messaging_client> _data_line = nullptr;
shared_ptr<messaging_client> _file_line = nullptr;
shared_ptr<messaging_server> _middle_server = nullptr;

#ifdef _CONSOLE
BOOL ctrl_handler(DWORD ctrl_type);
#endif

void parse_bool(const wstring& key, argument_manager& arguments, bool& value);
void parse_ushort(const wstring& key, argument_manager& arguments, unsigned short& value);
#ifdef _WIN32
void parse_ullong(const wstring& key, argument_manager& arguments, unsigned long long& value);
#else
void parse_ulong(const wstring& key, argument_manager& arguments, unsigned long& value);
#endif
void parse_string(const wstring& key, argument_manager& arguments, wstring& value);
bool parse_arguments(argument_manager& arguments);
void display_help(void);

void create_middle_server(void);
void create_data_line(void);
void create_file_line(void);
void connection_from_middle_server(const wstring& target_id, const wstring& target_sub_id, const bool& condition);
void connection_from_data_line(const wstring& target_id, const wstring& target_sub_id, const bool& condition);
void connection_from_file_line(const wstring& target_id, const wstring& target_sub_id, const bool& condition);

void received_message_from_middle_server(shared_ptr<container::value_container> container);
void received_message_from_data_line(shared_ptr<container::value_container> container);
void received_message_from_file_line(shared_ptr<container::value_container> container);

void received_file_from_file_line(const wstring& source_id, const wstring& source_sub_id, const wstring& indication_id, const wstring& target_path);

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

#ifdef _CONSOLE
	SetConsoleCtrlHandler((PHANDLER_ROUTINE)ctrl_handler, TRUE);
#endif

	_file_commands.insert({ L"download_files", &download_files });
	_file_commands.insert({ L"upload_files", &upload_files });

	logger::handle().set_write_console(write_console, write_console_only);
	logger::handle().set_target_level(log_level);
	logger::handle().start(PROGRAM_NAME);

	_file_manager = make_shared<file_manager>();

	create_middle_server();
	create_data_line();
	create_file_line();

	_middle_server->wait_stop();

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
		{
			_middle_server.reset();
			_data_line.reset();
			_file_line.reset();

			logger::handle().stop();
		}
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

void parse_string(const wstring& key, argument_manager& arguments, wstring& value)
{
	auto target = arguments.get(key);
	if(!target.empty())
	{
		value = target;
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

	target = arguments.get(L"--main_connection_key");
	if (!target.empty())
	{
		temp = converter::to_wstring(file::load(target));
		if (!temp.empty())
		{
			main_connection_key = temp;
		}
	}

	target = arguments.get(L"--middle_connection_key");
	if (!target.empty())
	{
		temp = converter::to_wstring(file::load(target));
		if (!temp.empty())
		{
			middle_connection_key = temp;
		}
	}
	
	parse_string(L"--main_server_ip", arguments, main_server_ip);
	parse_ushort(L"--main_server_port", arguments, main_server_port);
	parse_ushort(L"--middle_server_port", arguments, middle_server_port);
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
	_middle_server->set_message_notification(&received_message_from_middle_server);
	_middle_server->start(middle_server_port, high_priority_count, normal_priority_count, low_priority_count);
}

void create_data_line(void)
{
	if (_data_line != nullptr)
	{
		_data_line.reset();
	}

	_data_line = make_shared<messaging_client>(L"data_line");
	_data_line->set_bridge_line(true);
	_data_line->set_compress_mode(compress_mode);
	_data_line->set_connection_key(main_connection_key);
	_data_line->set_session_types(session_types::message_line);
	_data_line->set_connection_notification(&connection_from_data_line);
	_data_line->set_message_notification(&received_message_from_data_line);
	_data_line->start(main_server_ip, main_server_port, high_priority_count, normal_priority_count, low_priority_count);
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
	_file_line->start(main_server_ip, main_server_port, high_priority_count, normal_priority_count, low_priority_count);
}

void connection_from_middle_server(const wstring& target_id, const wstring& target_sub_id, const bool& condition)
{
	logger::handle().write(logging_level::information,
		fmt::format(L"a client on middle server: {}[{}] is {}", target_id, target_sub_id, condition ? L"connected" : L"disconnected"));
}

void received_message_from_middle_server(shared_ptr<container::value_container> container)
{
	if (container == nullptr)
	{
		return;
	}

	auto target = _file_commands.find(container->message_type());
	if (target != _file_commands.end())
	{
		target->second(container);

		return;
	}

	if (_data_line == nullptr || _data_line->get_confirm_status() == connection_conditions::confirmed)
	{
		if (_middle_server == nullptr)
		{
			return;
		}

		shared_ptr<container::value_container> response = container->copy(false);
		response->swap_header();

		response << make_shared<container::bool_value>(L"error", true);
		response << make_shared<container::string_value>(L"reason", L"main_server has not been connected.");

		_middle_server->send(response);

		return;
	}

	if (_data_line)
	{
		_data_line->send(container);
	}
}

void connection_from_data_line(const wstring& target_id, const wstring& target_sub_id, const bool& condition)
{
	if (_data_line == nullptr)
	{
		return;
	}

	logger::handle().write(logging_level::sequence,
		fmt::format(L"{} on middle server is {} from target: {}[{}]", _data_line->source_id(), condition ? L"connected" : L"disconnected", target_id, target_sub_id));

	if (condition)
	{
		return;
	}

	if (_middle_server == nullptr)
	{
		return;
	}

	this_thread::sleep_for(chrono::seconds(1));

	_data_line->start(main_server_ip, main_server_port, high_priority_count, normal_priority_count, low_priority_count);
}

void connection_from_file_line(const wstring& target_id, const wstring& target_sub_id, const bool& condition)
{
	if (_file_line == nullptr)
	{
		return;
	}

	logger::handle().write(logging_level::sequence,
		fmt::format(L"{} on middle server is {} from target: {}[{}]", _file_line->source_id(), condition ? L"connected" : L"disconnected", target_id, target_sub_id));

	if (condition)
	{
		return;
	}

	if (_middle_server == nullptr)
	{
		return;
	}

	this_thread::sleep_for(chrono::seconds(1));

	_file_line->start(main_server_ip, main_server_port, high_priority_count, normal_priority_count, low_priority_count);
}

void received_message_from_data_line(shared_ptr<container::value_container> container)
{
	if (container == nullptr)
	{
		return;
	}

	if (_middle_server)
	{
		_middle_server->send(container);
	}
}

void received_message_from_file_line(shared_ptr<container::value_container> container)
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

void received_file_from_file_line(const wstring& target_id, const wstring& target_sub_id, const wstring& indication_id, const wstring& target_path)
{
	logger::handle().write(logging_level::parameter,
		fmt::format(L"target_id: {}, target_sub_id: {}, indication_id: {}, file_path: {}", target_id, target_sub_id, indication_id, target_path));

	shared_ptr<container::value_container> container = _file_manager->received(indication_id, target_path);

	if(container != nullptr)
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

	if (_file_line == nullptr || _file_line->get_confirm_status() == connection_conditions::confirmed)
	{
		if (_middle_server)
		{
			return;
		}

		shared_ptr<container::value_container> response = container->copy(false);
		response->swap_header();

		response << make_shared<container::bool_value>(L"error", true);
		response << make_shared<container::string_value>(L"reason", L"main_server has not been connected.");

		_middle_server->send(response);

		return;
	}

	vector<wstring> target_paths;

	vector<shared_ptr<container::value>> files = container->value_array(L"file");
	for (auto& file : files)
	{
		target_paths.push_back((*file)[L"target"]->to_string());
	}

	_file_manager->set(container->get_value(L"indication_id")->to_string(),
		container->source_id(), container->source_sub_id(), target_paths);

	if (_middle_server)
	{
		_middle_server->send(make_shared<container::value_container>(container->source_id(), container->source_sub_id(), L"transfer_condition",
			vector<shared_ptr<container::value>> {
				make_shared<container::string_value>(L"indication_id", container->get_value(L"indication_id")->to_string()),
				make_shared<container::ushort_value>(L"percentage", 0)
		}));
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

	if (_file_line == nullptr || _file_line->get_confirm_status() == connection_conditions::confirmed)
	{
		if (_middle_server)
		{
			return;
		}

		shared_ptr<container::value_container> response = container->copy(false);
		response->swap_header();

		response << make_shared<container::bool_value>(L"error", true);
		response << make_shared<container::string_value>(L"reason", L"main_server has not been connected.");

		_middle_server->send(response);

		return;
	}
	
	if (_file_line)
	{
		container << make_shared<container::string_value>(L"gateway_source_id", container->source_id());
		container << make_shared<container::string_value>(L"gateway_source_sub_id", container->source_sub_id());
		container->set_source(_file_line->source_id(), _file_line->source_sub_id());

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
		container->get_value(L"indication_id")->to_string(), container->get_value(L"target_path")->to_string());

	if (temp != nullptr)
	{
		if (_middle_server)
		{
			_middle_server->send(temp);
		}
	}
}

void display_help(void)
{
	wcout << L"main server options:" << endl << endl;
	wcout << L"--encrypt_mode [value] " << endl;
	wcout << L"\tThe encrypt_mode on/off. If you want to use encrypt mode must be appended '--encrypt_mode true'.\n\tInitialize value is --encrypt_mode off." << endl << endl;
	wcout << L"--compress_mode [value]" << endl;
	wcout << L"\tThe compress_mode on/off. If you want to use compress mode must be appended '--compress_mode true'.\n\tInitialize value is --compress_mode off." << endl << endl;
	wcout << L"--compress_block_size [value]" << endl;
	wcout << L"\tThe compress_mode on/off. If you want to change compress block size must be appended '--compress_block_size size'.\n\tInitialize value is --compress_mode 1024." << endl << endl;
	wcout << L"--main_connection_key [value]" << endl;
	wcout << L"\tIf you want to change a specific key string for the connection to the main server must be appended\n\t'--main_connection_key [specific key string]'." << endl << endl;
	wcout << L"--middle_connection_key [value]" << endl;
	wcout << L"\tIf you want to change a specific key string for the connection to the middle server must be appended\n\t'--middle_connection_key [specific key string]'." << endl << endl;
	wcout << L"--main_server_port [value]" << endl;
	wcout << L"\tIf you want to change a port number for the connection to the main server must be appended\n\t'--main_server_port [port number]'." << endl << endl;
	wcout << L"--middle_server_port [value]" << endl;
	wcout << L"\tIf you want to change a port number for the connection to the middle server must be appended\n\t'--middle_server_port [port number]'." << endl << endl;
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
