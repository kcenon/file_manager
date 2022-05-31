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
#include "cpprest/json.h"
#include "cpprest/http_client.h"

#include "fmt/xchar.h"
#include "fmt/format.h"

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

bool write_console = false;

#ifdef _DEBUG
logging_level log_level = logging_level::parameter;
#else
logging_level log_level = logging_level::information;
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
bool parse_arguments(argument_manager& arguments);
void display_help(void);

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

		display_help();

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
	container[MESSAGE_TYPE] = json::value::string("download_files");
	container[INDICATION_ID] = json::value::string("download_test");
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
	container[FILES] = json::value::array();
	for (auto& source : sources)
	{
		container[FILES][index][SOURCE] = json::value::string(converter::to_string(source));
		container[FILES][index][TARGET] = json::value::string(converter::to_string(converter::replace2(source, source_folder, target_folder)));
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
							converter::to_wstring(fmt::format("started {}: [{}]", message[MESSAGE_TYPE].as_string(),
								message[INDICATION_ID].as_string())));
						
						continue;
					}

					logger::handle().write(logging_level::information,
						converter::to_wstring(fmt::format("received percentage: [{}] {}%", message[INDICATION_ID].as_string(),
							message["percentage"].as_integer())));

					if (message["percentage"].as_integer() != 100)
					{
						continue;
					}

					if (message["completed"].as_bool())
					{
						logger::handle().write(logging_level::information,
							converter::to_wstring(fmt::format("completed {}: [{}]", message[MESSAGE_TYPE].as_string(),
								message[INDICATION_ID].as_string())));
					
						_promise_status.set_value(true);

						return;
					}

					logger::handle().write(logging_level::information,
						converter::to_wstring(fmt::format("cannot complete {}: [{}]", message[MESSAGE_TYPE].as_string(),
							message[INDICATION_ID].as_string())));

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

bool parse_arguments(argument_manager& arguments)
{
	wstring temp;

	auto target = arguments.get(L"--help");
	if (!target.empty())
	{
		display_help();

		return false;
	}

	target = arguments.get(L"--server_port");
	if (!target.empty())
	{
		server_port = (unsigned short)atoi(converter::to_string(target).c_str());
	}

	target = arguments.get(L"--source_folder");
	if (!target.empty())
	{
		source_folder = target;
	}

	target = arguments.get(L"--target_folder");
	if (!target.empty())
	{
		target_folder = target;
	}

	target = arguments.get(L"--write_console_mode");
	if (!target.empty())
	{
		temp = target;
		transform(temp.begin(), temp.end(), temp.begin(), ::tolower);

		if (temp.compare(L"true") == 0)
		{
			write_console = true;
		}
		else
		{
			write_console = false;
		}
	}

	target = arguments.get(L"--logging_level");
	if (!target.empty())
	{
		log_level = (logging_level)atoi(converter::to_string(target).c_str());
	}

	return true;
}

void display_help(void)
{
	wcout << L"restapi client sample options:" << endl << endl;
	wcout << L"--server_port [value]" << endl;
	wcout << L"\tIf you want to change a port number for the connection to the main server must be appended\n\t'--server_port [port number]'." << endl << endl;
	wcout << L"--source_folder [path]" << endl;
	wcout << L"\tIf you want to download folder on middle server on computer must be appended '--source_folder [path]'." << endl << endl;
	wcout << L"--target_folder [path]" << endl;
	wcout << L"\tIf you want to download on your computer must be appended '--target_folder [path]'." << endl << endl;
	wcout << L"--write_console_mode [value] " << endl;
	wcout << L"\tThe write_console_mode on/off. If you want to display log on console must be appended '--write_console_mode true'.\n\tInitialize value is --write_console_mode off." << endl << endl;
	wcout << L"--logging_level [value]" << endl;
	wcout << L"\tIf you want to change log level must be appended '--logging_level [level]'." << endl;
}
