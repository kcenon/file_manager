#include "file_manager.h"

#include "converting.h"
#include "constexpr_string.h"

#ifdef __USE_TYPE_CONTAINER__
#include "value.h"
#include "values/bool_value.h"
#include "values/ushort_value.h"
#include "values/ullong_value.h"
#include "values/string_value.h"
#endif

using namespace converting;

file_manager::file_manager(void)
{

}

file_manager::~file_manager(void)
{

}

bool file_manager::set(const wstring& indication_id, const wstring& source_id, const wstring& source_sub_id, 
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

#ifndef __USE_TYPE_CONTAINER__
shared_ptr<json::value> file_manager::received(const wstring& indication_id, const wstring& file_path)
#else
shared_ptr<container::value_container> file_manager::received(const wstring& indication_id, const wstring& file_path)
#endif
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

	unsigned short temp = (unsigned short)(((double)target->second.size() / (double)source->second.size()) * 100);
	if (percentage->second != temp)
	{
		percentage->second = temp;

#ifndef __USE_TYPE_CONTAINER__
		shared_ptr<json::value> container = make_shared<json::value>(json::value::object(true));

#ifdef _WIN32
		(*container)[HEADER][TARGET_ID] = json::value::string(ids->second.first);
		(*container)[HEADER][TARGET_SUB_ID] = json::value::string(ids->second.second);
		(*container)[HEADER][MESSAGE_TYPE] = json::value::string(TRANSFER_CONDITON);

		(*container)[DATA][INDICATION_ID] = json::value::string(indication_id);
		(*container)[DATA][L"percentage"] = json::value::number(temp);
#else
		(*container)[HEADER][TARGET_ID] = json::value::string(converter::to_string(ids->second.first));
		(*container)[HEADER][TARGET_SUB_ID] = json::value::string(converter::to_string(ids->second.second));
		(*container)[HEADER][MESSAGE_TYPE] = json::value::string(TRANSFER_CONDITON);

		(*container)[DATA][INDICATION_ID] = json::value::string(converter::to_string(indication_id));
		(*container)[DATA]["percentage"] = json::value::number(temp);
#endif

		if (temp == 100)
		{
			size_t completed = target->second.size();
			size_t failed = fail->second.size();

			_transferring_list.erase(source);
			_transferring_ids.erase(ids);
			_transferred_list.erase(target);
			_failed_list.erase(fail);
			_transferred_percentage.erase(percentage);

#ifdef _WIN32
			(*container)[DATA][L"completed_count"] = json::value::number((uint64_t)completed);
			(*container)[DATA][L"failed_count"] = json::value::number((uint64_t)failed);
			(*container)[DATA][L"completed"] = json::value::boolean(true);
#else
			(*container)[DATA]["completed_count"] = json::value::number((uint64_t)completed);
			(*container)[DATA]["failed_count"] = json::value::number((uint64_t)failed);
			(*container)[DATA]["completed"] = json::value::boolean(true);
#endif
		}

		return container;
#else
		if (temp != 100)
		{
			return make_shared<container::value_container>(ids->second.first, ids->second.second, TRANSFER_CONDITON,
				vector<shared_ptr<container::value>> {
					make_shared<container::string_value>(INDICATION_ID, indication_id),
					make_shared<container::ushort_value>(L"percentage", temp)
			});
		}

		size_t completed = target->second.size();
		size_t failed = fail->second.size();

		return make_shared<container::value_container>(ids->second.first, ids->second.second, TRANSFER_CONDITON,
			vector<shared_ptr<container::value>> {
				make_shared<container::string_value>(INDICATION_ID, indication_id),
				make_shared<container::ushort_value>(L"percentage", temp),
				make_shared<container::ullong_value>(L"completed_count", completed),
				make_shared<container::ullong_value>(L"failed_count", failed),
				make_shared<container::bool_value>(L"completed", true)
		});
#endif
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

		_transferring_list.erase(source);
		_transferring_ids.erase(ids);
		_transferred_list.erase(target);
		_failed_list.erase(fail);
		_transferred_percentage.erase(percentage);

#ifndef __USE_TYPE_CONTAINER__
		shared_ptr<json::value> container = make_shared<json::value>(json::value::object(true));

#ifdef _WIN32
		(*container)[HEADER][TARGET_ID] = json::value::string(source_id);
		(*container)[HEADER][TARGET_SUB_ID] = json::value::string(source_sub_id);
		(*container)[HEADER][MESSAGE_TYPE] = json::value::string(TRANSFER_CONDITON);

		(*container)[DATA][INDICATION_ID] = json::value::string(indication_id);
		(*container)[DATA][L"percentage"] = json::value::number(temp);
		(*container)[DATA][L"completed_count"] = json::value::number((uint64_t)completed);
		(*container)[DATA][L"failed_count"] = json::value::number((uint64_t)failed);
		(*container)[DATA][L"completed"] = json::value::boolean(false);
#else
		(*container)[HEADER][TARGET_ID] = json::value::string(converter::to_string(source_id));
		(*container)[HEADER][TARGET_SUB_ID] = json::value::string(converter::to_string(source_sub_id));
		(*container)[HEADER][MESSAGE_TYPE] = json::value::string(TRANSFER_CONDITON);

		(*container)[DATA][INDICATION_ID] = json::value::string(converter::to_string(indication_id));
		(*container)[DATA]["percentage"] = json::value::number(temp);
		(*container)[DATA]["completed_count"] = json::value::number((uint64_t)completed);
		(*container)[DATA]["failed_count"] = json::value::number((uint64_t)failed);
		(*container)[DATA]["completed"] = json::value::boolean(false);
#endif

		return container;
#else
		return make_shared<container::value_container>(source_id, source_sub_id, TRANSFER_CONDITON,
			vector<shared_ptr<container::value>> {
				make_shared<container::string_value>(INDICATION_ID, indication_id),
				make_shared<container::ushort_value>(L"percentage", temp),
				make_shared<container::ullong_value>(L"completed_count", completed),
				make_shared<container::ullong_value>(L"failed_count", failed),
				make_shared<container::bool_value>(L"completed", true)
		});
#endif
	}

	return nullptr;
}