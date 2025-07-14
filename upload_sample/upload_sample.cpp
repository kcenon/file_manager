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
#include <future>

#include "utilities/parsing/argument_parser.h"
#include "utilities/conversion/convert_string.h"
#include "utilities/io/file_handler.h"
#include "logger/core/logger.h"
#include "network/network.h"

#include "container/container.h"
#include "values/container_value.h"
#include "values/string_value.h"

#include "fmt/format.h"
#include "fmt/xchar.h"

#include <future>

constexpr auto PROGRAM_NAME = "upload_sample";

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

std::wstring source_folder = L"";
std::wstring target_folder = L"";
std::wstring connection_key = L"middle_connection_key";
std::wstring server_ip = L"127.0.0.1";
unsigned short server_port = 8642;
unsigned short high_priority_count = 1;
unsigned short normal_priority_count = 2;
unsigned short low_priority_count = 3;

std::promise<bool> _promise_status;
std::future<bool> _future_status;
std::shared_ptr<messaging_client> client = nullptr;

std::map<std::string, std::function<void(std::shared_ptr<value_container>)>>
	_registered_messages;

bool parse_arguments(argument_manager& arguments);
void connection(const std::wstring& target_id,
				const std::wstring& target_sub_id,
				const bool& condition);

void received_message(std::shared_ptr<value_container> container);
void transfer_condition(std::shared_ptr<value_container> container);
void request_upload_files(void);

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

	log_module::set_title(PROGRAM_NAME);
	if (write_console) {
		log_module::console_target(log_level);
	}
	log_module::file_target(log_level);
	log_module::start();

	// TODO: folder::get_files() is not available in current messaging_system structure
	// Need to implement directory traversal using std::filesystem or provide alternative
	vector<wstring> sources; // = folder::get_files(source_folder);
	if (sources.empty())
	{
		log_module::stop();

		return 0;
	}

	_registered_messages.insert({ "transfer_condition", transfer_condition });

	client = std::make_shared<messaging_client>(PROGRAM_NAME);
	// TODO: set_compress_mode, set_connection_key, set_session_types, set_connection_notification, set_message_notification APIs are not available in the new messaging_client
	// Need to implement these features or use alternative approach
	client->start_client(std::get<0>(convert_string::to_string(server_ip)).value_or(""), server_port);

	_future_status = _promise_status.get_future();
	_future_status.wait();

	client->stop_client();

	log_module::stop();

	return 0;
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

	auto ushort_target = arguments.to_ushort("--server_port");
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

void connection(const std::wstring& target_id,
				const std::wstring& target_sub_id,
				const bool& condition)
{
	auto [tid_str, tid_err] = convert_string::to_string(target_id);
	auto [tsid_str, tsid_err] = convert_string::to_string(target_sub_id);
	log_module::write_information(
		fmt::format("a client on main server: {}[{}] is {}", tid_str.value_or(""),
					tsid_str.value_or(""), condition ? "connected" : "disconnected").c_str());

	if (condition)
	{
		request_upload_files();
	}
}

void received_message(std::shared_ptr<value_container> container)
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

	log_module::write_sequence(
		fmt::format("unknown message: {}", container->serialize()).c_str());
}

void transfer_condition(std::shared_ptr<value_container> container)
{
	if (container == nullptr)
	{
		return;
	}

	if (container->message_type() != "transfer_condition")
	{
		return;
	}

	if (container->get_value("percentage")->to_ushort() == 0)
	{
		log_module::write_information(
			fmt::format("started upload: [{}]",
						container->get_value("indication_id")->to_string()).c_str());

		return;
	}

	log_module::write_information(
		fmt::format("received percentage: [{}] {}%",
					container->get_value("indication_id")->to_string(),
					container->get_value("percentage")->to_ushort()).c_str());

	if (container->get_value("completed")->to_boolean())
	{
		log_module::write_information(
			fmt::format("completed upload: [{}]",
						container->get_value("indication_id")->to_string()).c_str());

		_promise_status.set_value(true);

		return;
	}

	if (container->get_value("percentage")->to_ushort() == 100)
	{
		log_module::write_information(
			fmt::format("completed upload: [{}] success-{}, fail-{}",
						container->get_value("indication_id")->to_string(),
						container->get_value("completed_count")->to_ushort(),
						container->get_value("failed_count")->to_ushort()).c_str());

		_promise_status.set_value(false);
	}
}

void request_upload_files(void)
{
	// TODO: folder::get_files() is not available in current messaging_system structure
	// Need to implement directory traversal using std::filesystem or provide alternative
	std::vector<std::wstring> sources; // = folder::get_files(source_folder);
	if (sources.empty())
	{
		auto [sf_str, sf_err] = convert_string::to_string(source_folder);
		log_module::write_error(
			fmt::format("there is no file: {}", sf_str.value_or("")).c_str());

		return;
	}

	std::vector<std::shared_ptr<value>> files;

	files.push_back(
		std::make_shared<string_value>("indication_id", "upload_test"));
	for (auto& source : sources)
	{
		auto [src_str, src_err] = convert_string::to_string(source);
		// TODO: convert_string::replace API needs to be checked
		files.push_back(std::make_shared<container_value>(
			"file",
			std::vector<std::shared_ptr<value>>{
				std::make_shared<string_value>("source", src_str.value_or("")),
				std::make_shared<string_value>("target", "") }));
	}

	std::shared_ptr<value_container> container
		= std::make_shared<value_container>("main_server", "",
												  "upload_files", files);

	// TODO: client->send(container) API is not available in the new messaging_client
	// Need to convert container to bytes and use send_packet
	// client->send_packet(container_to_bytes(container));
}
