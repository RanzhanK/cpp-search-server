#pragma once

#include <map>
#include <set>
#include <string>
#include <cmath>
#include <deque>
#include <vector>
#include <numeric>
#include <utility>
#include <iostream>
#include <algorithm>
#include <stdexcept>

#include "document.h"
#include "search_server.h"
#include "string_processing.h"

//метод поиска и удаления дубликатов
void RemoveDuplicates(SearchServer &search_server);