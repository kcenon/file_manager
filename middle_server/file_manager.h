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

#pragma once

#include "container/container.h"
#include "core/value.h"
#include "values/string_value.h"

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

using namespace std;
using namespace container_module;

class file_manager
{
public:
	file_manager(void);
	~file_manager(void);

public:
	bool set(const wstring& indication_id,
			 const wstring& source_id,
			 const wstring& source_sub_id,
			 const vector<wstring>& file_list);

	shared_ptr<value_container> received(
		const wstring& indication_id, const wstring& file_path);

private:
	void clear(const map<wstring, unsigned short>::iterator& percentage_iter,
			   const map<wstring, pair<wstring, wstring>>::iterator& ids_iter,
			   const map<wstring, vector<wstring>>::iterator& transferring_iter,
			   const map<wstring, vector<wstring>>::iterator& transferred_iter,
			   const map<wstring, vector<wstring>>::iterator& failed_iter);

private:
	mutex _mutex;
	map<wstring, unsigned short> _transferred_percentage;
	map<wstring, pair<wstring, wstring>> _transferring_ids;
	map<wstring, vector<wstring>> _transferring_list;
	map<wstring, vector<wstring>> _transferred_list;
	map<wstring, vector<wstring>> _failed_list;
};
