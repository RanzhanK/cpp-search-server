#include <list>
#include <vector>
#include <string>
#include <iostream>
#include <algorithm>
#include <execution>
#include <functional>

#include "process_queries.h"

//метод распаралеливания нескольких запросов к серверу
std::vector <std::vector<Document>> ProcessQueries(
        const SearchServer &search_server,
        const std::vector <std::string> &queries) {

    // Вектор с заранее созданным размером
    std::vector <std::vector<Document>> result(queries.size());

    // Подсчитываем функцией transform количество искомых слов
    // при этом используем execution::par, для распаралеливания запроса
    transform(std::execution::par, queries.begin(), queries.end(), result.begin(),
              [&search_server](std::string query_find) {
                  return search_server.FindTopDocuments(query_find);
              });

    return result;
}

//метод распаралеливания нескольких запросов к серверу
//возвращает плоский набор документов
std::list <Document> ProcessQueriesJoined(
        const SearchServer &search_server,
        const std::vector <std::string> &queries) {

    // Cуммарный размер внутренних векторов FindTopDocuments
    auto matched_docs = ProcessQueries(search_server, queries);

    std::list <Document> result;
    for (auto &query_res: matched_docs) {
        result.insert(result.end(), query_res.begin(), query_res.end());
    }

    return result;
}
