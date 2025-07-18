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

#include "file_manager.h"

#include "utilities/conversion/convert_string.h"

#include "container/core/value.h"
#include "container/values/bool_value.h"
#include "container/values/string_value.h"
#include "container/values/numeric_value.h"
#include "container/core/value_types.h"

using namespace utility_module;
using namespace container_module;

file_manager::file_manager(void) {}

file_manager::~file_manager(void) {}

bool file_manager::set(const wstring& indication_id,
					   const wstring& source_id,
					   const wstring& source_sub_id,
					   const vector<wstring>& file_list)
{
	scoped_lock<mutex> guard(_mutex);

	auto target = _transferring_list.find(indication_id);
	if (target != _transferring_list.end())
	{
		return false;
	}

	_transferring_list.insert({ indication_id, file_list });
	_transferring_ids.insert({ indication_id, { source_id, source_sub_id } });
	_transferred_list.insert({ indication_id, vector<wstring>() });
	_failed_list.insert({ indication_id, vector<wstring>() });
	_transferred_percentage.insert({ indication_id, 0 });

	return true;
}

shared_ptr<value_container> file_manager::received(
	const wstring& indication_id, const wstring& file_path)
{
	scoped_lock<mutex> guard(_mutex);

	auto ids = _transferring_ids.find(indication_id);
	if (ids == _transferring_ids.end())
	{
		return nullptr;
	}

	auto source = _transferring_list.find(indication_id);
	if (source == _transferring_list.end())
	{
		return nullptr;
	}

	auto target = _transferred_list.find(indication_id);
	if (target == _transferred_list.end())
	{
		return nullptr;
	}

	auto fail = _failed_list.find(indication_id);
	if (fail == _failed_list.end())
	{
		return nullptr;
	}

	auto percentage = _transferred_percentage.find(indication_id);
	if (percentage == _transferred_percentage.end())
	{
		return nullptr;
	}

	if (file_path.empty())
	{
		fail->second.push_back(file_path);
	}
	else
	{
		target->second.push_back(file_path);
	}

	unsigned short temp = (unsigned short)(((double)target->second.size()
											/ (double)source->second.size())
										   * 100);
	if (percentage->second != temp)
	{
		percentage->second = temp;

		if (temp != 100)
		{
			return make_shared<value_container>(
				[&]() -> std::string {
					auto [str, err] = convert_string::to_string(ids->second.first);
					return str.value_or("");
				}(),
				[&]() -> std::string {
					auto [str, err] = convert_string::to_string(ids->second.second);
					return str.value_or("");
				}(),
				"transfer_condition",
				vector<shared_ptr<value>>{
					make_shared<string_value>("indication_id",
														 [&]() -> std::string {
															 auto [str, err] = convert_string::to_string(std::wstring(indication_id));
															 return str.value_or("");
														 }()),
					make_shared<numeric_value<unsigned short, value_types::ushort_value>>("percentage",
														 temp) });
		}

		size_t completed = target->second.size();
		size_t failed = fail->second.size();

		clear(percentage, ids, source, target, fail);

		return make_shared<value_container>(
			[&]() -> std::string {
				auto [str, err] = convert_string::to_string(ids->second.first);
				return str.value_or("");
			}(),
			[&]() -> std::string {
				auto [str, err] = convert_string::to_string(ids->second.second);
				return str.value_or("");
			}(),
			"transfer_condition",
			vector<shared_ptr<value>>{
				make_shared<string_value>("indication_id",
													 std::get<0>(convert_string::to_string(std::wstring(indication_id))).value_or("")),
				make_shared<numeric_value<unsigned short, value_types::ushort_value>>("percentage", temp),
				make_shared<numeric_value<unsigned long long, value_types::ullong_value>>("completed_count",
													 completed),
				make_shared<numeric_value<unsigned long long, value_types::ullong_value>>("failed_count", failed),
				make_shared<bool_value>("completed", true) });
	}

	if (temp != 100)
	{
		return nullptr;
	}

	if (source->second.size() == (target->second.size() + fail->second.size()))
	{
		size_t completed = target->second.size();
		size_t failed = fail->second.size();
		wstring source_id = ids->second.first;
		wstring source_sub_id = ids->second.second;

		clear(percentage, ids, source, target, fail);

		return make_shared<value_container>(
			[&]() -> std::string {
				auto [str, err] = convert_string::to_string(source_id);
				return str.value_or("");
			}(),
			[&]() -> std::string {
				auto [str, err] = convert_string::to_string(source_sub_id);
				return str.value_or("");
			}(),
			"transfer_condition",
			vector<shared_ptr<value>>{
				make_shared<string_value>("indication_id",
													 std::get<0>(convert_string::to_string(std::wstring(indication_id))).value_or("")),
				make_shared<numeric_value<unsigned short, value_types::ushort_value>>("percentage", temp),
				make_shared<numeric_value<unsigned long long, value_types::ullong_value>>("completed_count",
													 completed),
				make_shared<numeric_value<unsigned long long, value_types::ullong_value>>("failed_count", failed),
				make_shared<bool_value>("completed", true) });
	}

	return nullptr;
}

void file_manager::clear(
	const map<wstring, unsigned short>::iterator& percentage_iter,
	const map<wstring, pair<wstring, wstring>>::iterator& ids_iter,
	const map<wstring, vector<wstring>>::iterator& transferring_iter,
	const map<wstring, vector<wstring>>::iterator& transferred_iter,
	const map<wstring, vector<wstring>>::iterator& failed_iter)
{
	_transferring_list.erase(transferring_iter);
	_transferring_ids.erase(ids_iter);
	_transferred_list.erase(transferred_iter);
	_failed_list.erase(failed_iter);
	_transferred_percentage.erase(percentage_iter);
}