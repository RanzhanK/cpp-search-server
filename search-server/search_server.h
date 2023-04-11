#pragma once

#include <map>
#include <set>
#include <deque>
#include <cmath>
#include <mutex>
#include <future>
#include <atomic>
#include <string>
#include <vector>
#include <utility>
#include <iostream>
#include <algorithm>
#include <stdexcept>
#include <execution>
#include <functional>
#include <string_view>

#include "document.h"
#include "log_duration.h"
#include "concurrent_map.h"
#include "string_processing.h"
#include "read_input_functions.h"

constexpr double PRECISION = 1e-6;
const int MAX_RESULT_DOCUMENT_COUNT = 5;
const unsigned int CPU_THREAD = std::thread::hardware_concurrency();

class SearchServer {
public:
    template <typename StringContainer>
    explicit SearchServer(const StringContainer &stop_words);
    explicit SearchServer( std::string_view stop_words_text);
    explicit SearchServer( const std::string &stop_words_text);

    //метод добавления документов
    void AddDocument(int document_id, std::string_view document, DocumentStatus status, const std::vector<int> &ratings);

    //метод поиска топ докуметов с лямбдой
    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const;
    //метод поиска топ докуметов с заданным статусом
    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentStatus status) const;
    //метод поиска топ докуметов с актуальным статусом
    std::vector<Document> FindTopDocuments(std::string_view raw_query) const;
    //однопоточный/паралельный метод поиска топ докуметов с лямбдой
    template <typename DocumentPredicate, typename Policy>
    std::vector<Document> FindTopDocuments(const Policy&, std::string_view raw_query, DocumentPredicate document_predicate) const;
    //однопоточный/паралельный метод поиска топ докуметов с заданным статусом
    template <typename Policy>
    std::vector<Document> FindTopDocuments(const Policy&, std::string_view raw_query, DocumentStatus status) const;
    //однопоточный/паралельный метод поиска топ докуметов с актуальным статусом
    template <typename Policy>
    std::vector<Document> FindTopDocuments(const Policy&, std::string_view raw_query) const;
    //метод возвращает все плюс-слова запроса, содержащиеся в документе отсортированые по возрастанию.
    //если нет пересечений по плюс-словам или есть минус-слово, вектор слов возвращается пустым.
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::string_view raw_query, int document_id) const;
    //однопоточный метод, возвращает результат работы предыдущей функции
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::execution::sequenced_policy&,  std::string_view raw_query, int document_id) const;
    //паралельный метод поиска плюс слов в документах
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::execution::parallel_policy& policy,  std::string_view raw_query, int document_id) const;

    //метод возвращает количество документов в поисковой системе.
    int GetDocumentCount() const ;

    //метод возвращает приватную переменную id документов
    //изменил все методв на set для хранения document_id
    std::set<int> GetDocumentId() {return document_id_;};

    std::set<int>::const_iterator begin() const;
    std::set<int>::const_iterator end() const;
    std::set<int>::iterator begin();
    std::set<int>::iterator end();

    //метод удаляет документ
    void RemoveDocument(int document_id);
    //однопоточный метод удаляет документ
    void RemoveDocument(const std::execution::sequenced_policy&, int document_id);
    //паралельный метод удаляет документ
    void RemoveDocument(const std::execution::parallel_policy& policy, int document_id);

    //метод получения частот слов по id документа
    const std::map<std::string_view, double> &GetWordFrequencies(int document_id) const;

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
    //id документов, изменил на set для хранения document_id
    std::set<int> document_id_;
    std::deque<std::string> deque_document_structure_ ;
    //структура документов
    std::map<int, DocumentData> documents_;
    //структура сохраняющая стоп слова
    const std::set<std::string, std::less<>> stop_words_;
    //структура сохраняющая частоту слов в документе
    std::map<int, std::map<std::string_view, double>> word_freqs_;
    //структура которая сопоставляет каждому слову словарь «документ → TF»
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;

    //метод проверки на стоп слово
    bool IsStopWord(std::string_view word) const;

    //метод проверки на валидность слов
    static bool IsValidWord(std::string_view word);

    //метод очищающий запрос от стоп слов
    std::vector<std::string_view> SplitIntoWordsNoStop(std::string_view text) const;

    //метод расчитывающий рейтинг слов
    static int ComputeAverageRating(const std::vector<int> &ratings);

    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(std::string_view text) const;

    //изменил структуру хранения слов с set на vector
    //для возможности итерироватся методом unique, и удалять дубликаты
    struct Query {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };

    //метод для парсинга плюс/минус слов
    Query ParseQuery(std::string_view  text) const;
    Query ParseQuery(const std::execution::sequenced_policy&, std::string_view text) const ;
    //паралельный метод для парсинга плюс/минус слов, с булевым флагом
    Query ParseQuery(bool flag, std::string_view text) const;

    double ComputeWordInverseDocumentFreq(const std::string &word) const;

    //метод поиска всех документов
    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const;
    //однопоточный метод поиска всех документов
    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const std::execution::sequenced_policy, const Query& query, DocumentPredicate document_predicate) const;
    //паралельный метод поиска всех документов
    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const std::execution::parallel_policy, const Query& query, DocumentPredicate document_predicate) const;
};

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
        : stop_words_(MakeUniqueNonEmptyStrings(stop_words))
{
    if (!all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
        throw std::invalid_argument("Some of stop words are invalid");
    }
}
template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const {
    return FindTopDocuments(std::execution::seq, raw_query, document_predicate);
}

template<typename DocumentPredicate, typename Policy>
std::vector<Document> SearchServer::FindTopDocuments(const Policy &policy, std::string_view raw_query, DocumentPredicate document_predicate) const {
    const auto query = ParseQuery(raw_query);
    auto matched_documents = FindAllDocuments(policy, query, document_predicate);
    sort(policy, matched_documents.begin(), matched_documents.end(),
         [](const Document &lhs, const Document &rhs) {
             if (std::abs(lhs.relevance - rhs.relevance) < PRECISION) {
                 return lhs.rating > rhs.rating;
             }
             return lhs.relevance > rhs.relevance;
         });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return matched_documents;
}

template <typename Policy>
std::vector<Document> SearchServer::FindTopDocuments(const Policy &policy, std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(policy, raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
        return document_status == status;
    });
}

template <typename Policy>
std::vector<Document> SearchServer::FindTopDocuments(const Policy &policy, std::string_view raw_query) const {
    return FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const Query &query, DocumentPredicate document_predicate) const {
    std::map<int, double> document_to_relevance;
    for (std::string_view word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        std::string word_str(word);
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word_str);
        for (const auto& [document_id, term_freq] : word_to_document_freqs_.at(word)) {
            const auto& document_data = documents_.at(document_id);
            if (document_predicate(document_id, document_data.status, document_data.rating)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }
    for ( std::string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        for (const auto& [document_id, _] : word_to_document_freqs_.at(word)) {
            document_to_relevance.erase(document_id);
        }
    }

    std::vector<Document> matched_documents;
    for (const auto& [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back({document_id, relevance, documents_.at(document_id).rating});
    }
    return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const std::execution::sequenced_policy, const Query& query, DocumentPredicate document_predicate) const {
    return SearchServer::FindAllDocuments(query, document_predicate);}


template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const std::execution::parallel_policy, const Query& query, DocumentPredicate document_predicate) const {
    ConcurrentMap<int, double> document_to_relevance(CPU_THREAD);
    for_each(
            std::execution::par,
            query.plus_words.begin(), query.plus_words.end(),
            [&, document_predicate](auto word ) {
                if (word_to_document_freqs_.count(word) != 0) {
                    std::string word_str(word);
                    const double inverse_document_freq = ComputeWordInverseDocumentFreq(word_str);
                    for (const auto& [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                        const auto& document_data = documents_.at(document_id);
                        if (document_predicate(document_id, document_data.status, document_data.rating))
                        {
                            document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
                        }
                    }}});
    for_each(
            std::execution::par,
            query.minus_words.begin(), query.minus_words.end(),
            [&](auto word ) {if (word_to_document_freqs_.count(word) != 0){
                for (const auto& [document_id, _] : word_to_document_freqs_.at(word)) {
                    document_to_relevance.Delete(document_id);
                }}});
    std::vector<Document> matched_documents;
    for (const auto& [document_id, relevance] : document_to_relevance.BuildOrdinaryMap()) {
        matched_documents.push_back({document_id, relevance, documents_.at(document_id).rating});
    }

    return matched_documents;
}