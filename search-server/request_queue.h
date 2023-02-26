#pragma once

#include <deque>
#include <vector>
#include <string>
#include <iostream>
#include "search_server.h"

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer &search_server)
            : search_server_(search_server), no_results_requests_(0), current_time_(0) {
    }

    //метод добавления запроса с лямбдой, для сохранения статистики
    template<typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string &raw_query, DocumentPredicate document_predicate);

    //метод добавления запроса с заданным статусом, для сохранения статистики
    std::vector<Document> AddFindRequest(const std::string &raw_query, DocumentStatus status);

    //метод добавления запроса с актуальным статусом, для сохранения статистики
    std::vector<Document> AddFindRequest(const std::string &raw_query);

    //метод для определения сколько за последние сутки было запросов, на которые ничего не нашлось
    int GetNoResultRequests() const;

private:
    struct QueryResult {
        uint64_t timestamp;
        int results;
    };

    std::deque<QueryResult> requests_;
    const SearchServer &search_server_;
    int no_results_requests_;
    uint64_t current_time_;
    const static int min_in_day_ = 1440;

    void AddRequest(int results_num);
};