#include "request_queue.h"

using namespace std;

//метод добавления запроса с лямбдой, для сохранения статистики
template<typename DocumentPredicate>
vector <Document> RequestQueue::AddFindRequest(const string &raw_query, DocumentPredicate document_predicate) {
    const auto result = search_server_.FindTopDocuments(raw_query, document_predicate);
    AddRequest(result.size());
    return result;
}

//метод добавления запроса с заданным статусом, для сохранения статистики
vector <Document> RequestQueue::AddFindRequest(const string &raw_query, DocumentStatus status) {
    const auto result = search_server_.FindTopDocuments(raw_query, status);
    AddRequest(result.size());
    return result;
}

//метод добавления запроса с актуальным статусом, для сохранения статистики
vector <Document> RequestQueue::AddFindRequest(const string &raw_query) {
    const auto result = search_server_.FindTopDocuments(raw_query);
    AddRequest(result.size());
    return result;
}

//метод для определения сколько за последние сутки было запросов, на которые ничего не нашлось
int RequestQueue::GetNoResultRequests() const {
    return no_results_requests_;
}

//добавляем запрос
void RequestQueue::AddRequest(int results_num) {
    ++current_time_;
    while (!requests_.empty() && min_in_day_ <= current_time_ - requests_.front().timestamp) {
        if (0 == requests_.front().results) {
            --no_results_requests_;
        }
        requests_.pop_front();
    }

    requests_.push_back({current_time_, results_num});
    if (0 == results_num) {
        ++no_results_requests_;
    }
}