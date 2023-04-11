#pragma once

#include <list>
#include <vector>
#include "document.h"
#include "search_server.h"

//метод распаралеливания нескольких запросов к серверу
std::vector <std::vector<Document>> ProcessQueries(
        const SearchServer &search_server,
        const std::vector <std::string> &queries);

//метод распаралеливания нескольких запросов к серверу
//возвращает плоский набор документов
std::list <Document> ProcessQueriesJoined(
        const SearchServer &search_server,
        const std::vector <std::string> &queries);