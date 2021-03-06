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
#include "converting.h"
#include "file_handler.h"
#include "messaging_client.h"
#include "folder_handler.h"
#include "argument_parser.h"

#include "container.h"
#include "values/string_value.h"
#include "values/container_value.h"

#include "fmt/xchar.h"
#include "fmt/format.h"

#include <future>

constexpr auto PROGRAM_NAME = L"upload_sample";

using namespace std;
using namespace logging;
using namespace network;
using namespace converting;
using namespace file_handler;
using namespace folder_handler;
using namespace argument_parser;

#ifdef _DEBUG
bool write_console = true;
#else
bool write_console = false;
#endif
bool write_console_only = false;
bool encrypt_mode = false;
bool compress_mode = true;
#ifdef _DEBUG
logging_level log_level = logging_level::parameter;
#else
logging_level log_level = logging_level::information;
#endif
wstring source_folder = L"";
wstring target_folder = L"";
wstring connection_key = L"middle_connection_key";
wstring server_ip = L"127.0.0.1";
unsigned short server_port = 8642;
unsigned short high_priority_count = 1;
unsigned short normal_priority_count = 2;
unsigned short low_priority_count = 3;

promise<bool> _promise_status;
future<bool> _future_status;
shared_ptr<messaging_client> client = nullptr;

map<wstring, function<void(shared_ptr<container::value_container>)>> _registered_messages;

void parse_bool(const wstring& key, argument_manager& arguments, bool& value);
void parse_ushort(const wstring& key, argument_manager& arguments, unsigned short& value);
bool parse_arguments(argument_manager& arguments);
void connection(const wstring& target_id, const wstring& target_sub_id, const bool& condition);

void received_message(shared_ptr<container::value_container> container);
void transfer_condition(shared_ptr<container::value_container> container);
void request_upload_files(void);

int main(int argc, char* argv[])
{
	argument_manager arguments(argc, argv);
	if (!parse_arguments(arguments))
	{
		return 0;
	}

	logger::handle().set_write_console(write_console);
	logger::handle().set_target_level(log_level);
	logger::handle().start(PROGRAM_NAME);

	vector<wstring> sources = folder::get_files(source_folder);
	if (sources.empty())
	{
		logger::handle().stop();

		return 0;
	}

	_registered_messages.insert({ L"transfer_condition", transfer_condition });

	client = make_shared<messaging_client>(PROGRAM_NAME);
	client->set_compress_mode(compress_mode);
	client->set_connection_key(connection_key);
	client->set_session_types(session_types::message_line);
	client->set_connection_notification(&connection);
	client->set_message_notification(&received_message);
	client->start(server_ip, server_port, high_priority_count, normal_priority_count, low_priority_count);

	_future_status = _promise_status.get_future();
	_future_status.wait();

	client->stop();

	logger::handle().stop();

	return 0;
}

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
	wstring target;

	parse_bool(L"--encrypt_mode", arguments, encrypt_mode);
	parse_bool(L"--compress_mode", arguments, compress_mode);

	target = arguments.get(L"--connection_key");
	if (!target.empty())
	{
		temp = converter::to_wstring(file::load(target));
		if (!temp.empty())
		{
			connection_key = temp;
		}
	}
	
	parse_string(L"--server_ip", arguments, server_ip);
	parse_ushort(L"--server_port", arguments, server_port);
	parse_string(L"--source_folder", arguments, source_folder);
	parse_string(L"--target_folder", arguments, target_folder);
	parse_ushort(L"--high_priority_count", arguments, high_priority_count);
	parse_ushort(L"--normal_priority_count", arguments, normal_priority_count);
	parse_ushort(L"--low_priority_count", arguments, low_priority_count);
	
	parse_bool(L"--write_console", arguments, write_console);
	parse_bool(L"--write_console_only", arguments, write_console_only);

	target = arguments.get(L"--logging_level");
	if (!target.empty())
	{
		log_level = (logging_level)atoi(converter::to_string(target).c_str());
	}

	return true;
}

void connection(const wstring& target_id, const wstring& target_sub_id, const bool& condition)
{
	logger::handle().write(logging_level::information,
		fmt::format(L"a client on main server: {}[{}] is {}", target_id, target_sub_id, 
			condition ? L"connected" : L"disconnected"));

	if (condition)
	{
		request_upload_files();
	}
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

	logger::handle().write(logging_level::sequence, fmt::format(L"unknown message: {}", container->serialize()));
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

	if (container->get_value(L"percentage")->to_ushort() == 0)
	{
		logger::handle().write(logging_level::information,
			fmt::format(L"started upload: [{}]", container->get_value(L"indication_id")->to_string()));

		return;
	}

	logger::handle().write(logging_level::information,
		fmt::format(L"received percentage: [{}] {}%", container->get_value(L"indication_id")->to_string(),
			container->get_value(L"percentage")->to_ushort()));

	if (container->get_value(L"completed")->to_boolean())
	{
		logger::handle().write(logging_level::information,
			fmt::format(L"completed upload: [{}]", container->get_value(L"indication_id")->to_string()));

		_promise_status.set_value(true);

		return;
	}

	if (container->get_value(L"percentage")->to_ushort() == 100)
	{
		logger::handle().write(logging_level::information,
			fmt::format(L"completed upload: [{}] success-{}, fail-{}", container->get_value(L"indication_id")->to_string(),
				container->get_value(L"completed_count")->to_ushort(), container->get_value(L"failed_count")->to_ushort()));

		_promise_status.set_value(false);
	}
}

void request_upload_files(void)
{
	vector<wstring> sources = folder::get_files(source_folder);
	if (sources.empty())
	{
		logger::handle().write(logging_level::error,
			fmt::format(L"there is no file: {}", source_folder));

		return;
	}

	vector<shared_ptr<container::value>> files;

	files.push_back(make_shared<container::string_value>(L"indication_id", L"upload_test"));
	for (auto& source : sources)
	{
		files.push_back(make_shared<container::container_value>(L"file", vector<shared_ptr<container::value>> {
			make_shared<container::string_value>(L"source", source),
			make_shared<container::string_value>(L"target", converter::replace2(source, source_folder, target_folder))
		}));
	}

	shared_ptr<container::value_container> container =
		make_shared<container::value_container>(L"main_server", L"", L"upload_files", files);

	client->send(container);
}
