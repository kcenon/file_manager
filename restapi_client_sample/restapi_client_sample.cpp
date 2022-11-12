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

#include "thread_pool.h"
#include "thread_worker.h"
#include "job_pool.h"
#include "job.h"

#include "converting.h"
#include "folder_handler.h"
#include "argument_parser.h"
#include "constexpr_string.h"

#include "fmt/xchar.h"
#include "fmt/format.h"

#include "cpprest/json.h"
#include "cpprest/http_client.h"

#include <future>
#include <vector>

constexpr auto PROGRAM_NAME = L"restapi_client_sample";

using namespace std;
using namespace logging;
using namespace threads;
using namespace converting;
using namespace folder_handler;
using namespace argument_parser;

using namespace web;
using namespace web::http;
using namespace web::http::client;

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
shared_ptr<http_client> _rest_client;

promise<bool> _promise_status;
future<bool> _future_status;

void get_request(void);
void post_request(const vector<unsigned char>& data);

void parse_bool(const wstring& key, argument_manager& arguments, bool& value);
void parse_ushort(const wstring& key, argument_manager& arguments, unsigned short& value);
void parse_string(const wstring& key, argument_manager& arguments, wstring& value);
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
	_thread_pool->append(make_shared<thread_worker>(priorities::high, vector<priorities> { priorities::normal, priorities::low }), true);
	_thread_pool->append(make_shared<thread_worker>(priorities::normal, vector<priorities> { priorities::high, priorities::low }), true);
	_thread_pool->append(make_shared<thread_worker>(priorities::low, vector<priorities> { priorities::high, priorities::normal }), true);

#ifdef _WIN32
	_rest_client = make_shared<http_client>(fmt::format(L"http://localhost:{}/restapi", server_port));
#else
	_rest_client = make_shared<http_client>(fmt::format("http://localhost:{}/restapi", server_port));
#endif

	_future_status = _promise_status.get_future();
	
	json::value container = json::value::object(true);

#ifdef _WIN32
	container[MESSAGE_TYPE] = json::value::string(L"download_files");
	container[INDICATION_ID] = json::value::string(L"download_test");
#else
	container[converter::to_string(MESSAGE_TYPE)] = json::value::string("download_files");
	container[converter::to_string(INDICATION_ID)] = json::value::string("download_test");
#endif

	int index = 0;
#ifdef _WIN32
	container[FILES] = json::value::array();
	for (auto& source : sources)
	{
		container[FILES][index][SOURCE] = json::value::string(source);
		container[FILES][index][TARGET] = json::value::string(converter::replace2(source, source_folder, target_folder));
		index++;
	}
#else
	container[converter::to_string(FILES)] = json::value::array();
	for (auto& source : sources)
	{
		container[converter::to_string(FILES)][index][converter::to_string(SOURCE)] = json::value::string(converter::to_string(source));
		container[converter::to_string(FILES)][index][converter::to_string(TARGET)] = json::value::string(converter::to_string(converter::replace2(source, source_folder, target_folder)));
		index++;
	}
#endif

	_thread_pool->push(make_shared<job>(priorities::high, converter::to_array(container.serialize()), &post_request));
	_thread_pool->push(make_shared<job>(priorities::low, &get_request));

	_future_status.wait();

	_thread_pool->stop();
	_thread_pool.reset();

	logger::handle().stop();

	return 0;
}

void get_request(void)
{
	http_request request(methods::GET);

#ifdef _WIN32
	request.headers().add(L"previous_message", L"clear");
	request.headers().add(INDICATION_ID, L"download_test");
#else
	request.headers().add("previous_message", "clear");
	request.headers().add("indication_id", "download_test");
#endif
	_rest_client->request(request)
		.then([](http_response response)
			{
				if (response.status_code() != status_codes::OK)
				{
					this_thread::sleep_for(chrono::seconds(1));

					_thread_pool->push(make_shared<job>(priorities::low, &get_request));

					return;
				}

				auto answer = response.extract_json().get();
				if (answer.is_null())
				{
					return;
				}

#ifdef _WIN32
				auto& messages = answer[L"messages"].as_array();
				for (auto& message : messages)
				{
					if (message[L"percentage"].as_integer() == 0)
					{
						logger::handle().write(logging_level::information,
							fmt::format(L"started {}: [{}]", message[MESSAGE_TYPE].as_string(), 
								message[INDICATION_ID].as_string()));

						continue;
					}

					logger::handle().write(logging_level::information,
						fmt::format(L"received percentage: [{}] {}%", message[INDICATION_ID].as_string(),
							message[L"percentage"].as_integer()));

					if (message[L"percentage"].as_integer() != 100)
					{
						continue;
					}

					if (message[L"completed"].as_bool())
					{
						logger::handle().write(logging_level::information,
							fmt::format(L"completed {}: [{}]", message[MESSAGE_TYPE].as_string(), 
								message[INDICATION_ID].as_string()));

						_promise_status.set_value(true);

						return;
					}

					logger::handle().write(logging_level::information,
						fmt::format(L"cannot complete {}: [{}]", message[MESSAGE_TYPE].as_string(), 
							message[INDICATION_ID].as_string()));

					_promise_status.set_value(false);

					return;
				}
#else
				auto& messages = answer["messages"].as_array();
				for (auto& message : messages)
				{
					if (message["percentage"].as_integer() == 0)
					{
						logger::handle().write(logging_level::information,
							converter::to_wstring(fmt::format("started {}: [{}]", 
								message[converter::to_string(MESSAGE_TYPE)].as_string(),
								message[converter::to_string(INDICATION_ID)].as_string())));
						
						continue;
					}

					logger::handle().write(logging_level::information,
						converter::to_wstring(fmt::format("received percentage: [{}] {}%", 
							message[converter::to_string(INDICATION_ID)].as_string(),
							message["percentage"].as_integer())));

					if (message["percentage"].as_integer() != 100)
					{
						continue;
					}

					if (message["completed"].as_bool())
					{
						logger::handle().write(logging_level::information,
							converter::to_wstring(fmt::format("completed {}: [{}]", 
								message[converter::to_string(MESSAGE_TYPE)].as_string(),
								message[converter::to_string(INDICATION_ID)].as_string())));
					
						_promise_status.set_value(true);

						return;
					}

					logger::handle().write(logging_level::information,
						converter::to_wstring(
							fmt::format("cannot complete {}: [{}]", 
								message[converter::to_string(MESSAGE_TYPE)].as_string(),
								message[converter::to_string(INDICATION_ID)].as_string())
							)
						);

					_promise_status.set_value(false);

					return;
				}
#endif

				_thread_pool->push(make_shared<job>(priorities::low, &get_request));
			})
		.wait();
}

void post_request(const vector<unsigned char>& data)
{
#ifdef _WIN32
	auto request_value = json::value::parse(converter::to_wstring(data));
#else
	auto request_value = json::value::parse(converter::to_string(data));
#endif

#ifdef _WIN32
	_rest_client->request(methods::POST, L"", request_value)
#else
	_rest_client->request(methods::POST, "", request_value)
#endif
		.then([](http_response response)
			{
				if (response.status_code() == status_codes::OK)
				{
#ifdef _WIN32
					logger::handle().write(logging_level::information, response.extract_string().get());
#else
					logger::handle().write(logging_level::information, converter::to_wstring(response.extract_string().get()));
#endif
				}
			})
		.wait();

	_thread_pool->push(make_shared<job>(priorities::low, &get_request));
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
	
	parse_ushort(L"--server_port", arguments, server_port);
	parse_string(L"--source_folder", arguments, source_folder);
	parse_string(L"--target_folder", arguments, target_folder);
	
	bool temp_condition = false;
	parse_bool(L"--write_console_only", arguments, temp_condition);
	if (temp_condition)
	{
		logging_style = logging_styles::console_only;
	}
	else
	{
		temp_condition = true;
		parse_bool(L"--write_console", arguments, temp_condition);
		if (temp_condition)
		{
			logging_style = logging_styles::file_and_console;
		}
		else
		{
			logging_style = logging_styles::file_only;
		}
	}

	target = arguments.get(L"--logging_level");
	if (!target.empty())
	{
		log_level = (logging_level)atoi(converter::to_string(target).c_str());
	}

	return true;
}