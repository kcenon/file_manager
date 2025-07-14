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

#include "job.h"
#include "job_pool.h"
#include "thread_pool.h"
#include "thread_worker.h"

#include "argument_parser.h"
#include "constexpr_string.h"
#include "converting.h"
#include "folder_handler.h"

#include "fmt/format.h"
#include "fmt/xchar.h"

// Using httplib and nlohmann_json instead of cpprestsdk
#include <httplib.h>
#include <nlohmann/json.hpp>

#include <future>
#include <vector>

constexpr auto PROGRAM_NAME = L"restapi_client_sample";

using namespace std;
using namespace logging;
using namespace threads;
using namespace converting;
using namespace folder_handler;
using namespace argument_parser;

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

wstring source_folder = L"";
wstring target_folder = L"";
unsigned short server_port = 7654;

shared_ptr<thread_pool> _thread_pool;
shared_ptr<httplib::Client> _rest_client;

promise<bool> _promise_status;
future<bool> _future_status;

void get_request(void);
void post_request(const vector<unsigned char>& data);

bool parse_arguments(argument_manager& arguments);

int main(int argc, char* argv[])
{
	argument_manager arguments(argc, argv);
	if (!parse_arguments(arguments))
	{
		return 0;
	}

	logger::handle().set_write_console(logging_style);
	logger::handle().set_target_level(log_level);
	logger::handle().start(PROGRAM_NAME);

	vector<wstring> sources = folder::get_files(source_folder);
	if (sources.empty())
	{
		logger::handle().stop();

		return 0;
	}

	_thread_pool = make_shared<threads::thread_pool>();
	_thread_pool->append(
		make_shared<thread_worker>(
			priorities::high,
			vector<priorities>{ priorities::normal, priorities::low }),
		true);
	_thread_pool->append(
		make_shared<thread_worker>(
			priorities::normal,
			vector<priorities>{ priorities::high, priorities::low }),
		true);
	_thread_pool->append(
		make_shared<thread_worker>(
			priorities::low,
			vector<priorities>{ priorities::high, priorities::normal }),
		true);

	// Create httplib client
	_rest_client = make_shared<httplib::Client>(
		fmt::format("http://localhost:{}", server_port));

	_future_status = _promise_status.get_future();

	// Using nlohmann::json
	json container;

	container["message_type"] = "download_files";
	container["indication_id"] = "download_test";
	container["files"] = json::array();

	int index = 0;
	for (auto& source : sources)
	{
		json file;
#ifdef _WIN32
		file["source"] = converter::to_string(source);
		file["target"] = converter::to_string(
			converter::replace2(source, source_folder, target_folder));
#else
		file["source"] = converter::to_string(source);
		file["target"] = converter::to_string(
			converter::replace2(source, source_folder, target_folder));
#endif
		container["files"].push_back(file);
		index++;
	}

	// Convert JSON to bytes and push jobs
	std::string json_str = container.dump();
	std::vector<unsigned char> json_bytes(json_str.begin(), json_str.end());

	_thread_pool->push(make_shared<job>(
		priorities::high, json_bytes, &post_request));
	_thread_pool->push(make_shared<job>(priorities::low, &get_request));

	_future_status.wait();

	_thread_pool->stop();
	_thread_pool.reset();

	logger::handle().stop();

	return 0;
}

void get_request(void)
{
	// Set headers
	httplib::Headers headers = {
		{"previous_message", "clear"},
		{"indication_id", "download_test"}
	};

	// Make GET request
	auto result = _rest_client->Get("/restapi", headers);
	
	if (!result || result->status != 200) {
		// If failed or status is not OK, retry after a delay
		this_thread::sleep_for(chrono::seconds(1));
		_thread_pool->push(make_shared<job>(priorities::low, &get_request));
		return;
	}

	// Parse JSON
	try {
		json answer = json::parse(result->body);
		if (answer.empty() || !answer.contains("messages")) {
			return;
		}

		auto& messages = answer["messages"];
		for (auto& message : messages) {
			if (message["percentage"] == 0) {
				logger::handle().write(
					logging_level::information,
					converter::to_wstring(fmt::format(
						"started {}: [{}]",
						message["message_type"].get<std::string>(),
						message["indication_id"].get<std::string>())));
				continue;
			}

			logger::handle().write(
				logging_level::information,
				converter::to_wstring(fmt::format(
					"received percentage: [{}] {}%",
					message["indication_id"].get<std::string>(),
					message["percentage"].get<int>())));

			if (message["percentage"] != 100) {
				continue;
			}

			if (message["completed"]) {
				logger::handle().write(
					logging_level::information,
					converter::to_wstring(fmt::format(
						"completed {}: [{}]",
						message["message_type"].get<std::string>(),
						message["indication_id"].get<std::string>())));

				_promise_status.set_value(true);
				return;
			}

			logger::handle().write(
				logging_level::information,
				converter::to_wstring(fmt::format(
					"cannot complete {}: [{}]",
					message["message_type"].get<std::string>(),
					message["indication_id"].get<std::string>())));

			_promise_status.set_value(false);
			return;
		}
	} catch (const std::exception& e) {
		logger::handle().write(
			logging_level::error,
			converter::to_wstring(fmt::format("JSON parsing error: {}", e.what())));
	}

	// Continue polling
	_thread_pool->push(make_shared<job>(priorities::low, &get_request));
}

void post_request(const vector<unsigned char>& data)
{
	// Convert bytes to string
	std::string json_str(data.begin(), data.end());
	
	// Parse JSON
	try {
		json request_value = json::parse(json_str);
		
		// Set headers for POST request
		httplib::Headers headers = {
			{"Content-Type", "application/json"}
		};
		
		// Make POST request
		auto result = _rest_client->Post("/restapi", headers, request_value.dump(), "application/json");
		
		if (result && result->status == 200) {
			logger::handle().write(
				logging_level::information,
				converter::to_wstring(result->body));
		}
	} catch (const std::exception& e) {
		logger::handle().write(
			logging_level::error,
			converter::to_wstring(fmt::format("JSON parsing error: {}", e.what())));
	}

	// Schedule next GET request
	_thread_pool->push(make_shared<job>(priorities::low, &get_request));
}

bool parse_arguments(argument_manager& arguments)
{
	auto ushort_target = arguments.to_ushort(L"--server_port");
	if (ushort_target != nullopt)
	{
		server_port = *ushort_target;
	}

	auto string_target = arguments.to_string(L"--source_folder");
	if (string_target != nullopt)
	{
		source_folder = *string_target;
	}

	string_target = arguments.to_string(L"--target_folder");
	if (string_target != nullopt)
	{
		target_folder = *string_target;
	}

	auto int_target = arguments.to_int(L"--logging_level");
	if (int_target != nullopt)
	{
		log_level = (logging_level)*int_target;
	}

	auto bool_target = arguments.to_bool(L"--write_console_only");
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