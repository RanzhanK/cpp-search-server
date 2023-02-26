#pragma once

#include <map>
#include <set>
#include <cmath>
#include <string>
#include <vector>
#include <numeric>
#include <utility>
#include <algorithm>
#include <stdexcept>
#include <algorithm>

#include "document.h"
#include "log_duration.h"
#include "string_processing.h"
#include "read_input_functions.h"

const int MAX_RESULT_DOCUMENT_COUNT = 5;
constexpr double PRECISION = 1e-6;

class SearchServer {
public:

    template<typename StringContainer>
    explicit SearchServer(const StringContainer &stop_words)
            : stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {
        auto word = SetStopWords(stop_words);
        if (!all_of(word.begin(), word.end(), IsValidWord)) {
            throw std::invalid_argument("invalid document: unacceptable symbols");
        }
    }

    explicit SearchServer(const std::string &stop_words_text);

    //метод добавления документов
    void
    AddDocument(int document_id, const std::string &document, DocumentStatus status, const std::vector<int> &ratings);

    //метод поиска топ докуметов с лямбдой
    template<typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::string &raw_query, DocumentPredicate document_predicate) const {
        LOG_DURATION("SearchServer::FindTopDocuments", std::cerr);
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, document_predicate);
        sort(matched_documents.begin(), matched_documents.end(),
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

    //метод поиска топ докуметов с заданным статусом
    std::vector<Document> FindTopDocuments(const std::string &raw_query, DocumentStatus status) const;

    //метод поиска топ докуметов с актуальным статусом
    std::vector<Document> FindTopDocuments(const std::string &raw_query) const;

    //метод возвращает все плюс-слова запроса, содержащиеся в документе отсортированые по возрастанию.
    //если нет пересечений по плюс-словам или есть минус-слово, вектор слов возвращается пустым.
    std::tuple<std::vector<std::string>, DocumentStatus>
    MatchDocument(const std::string &raw_query, int document_id) const;

    //метод возвращает количество документов в поисковой системе.
    int GetDocumentCount() const;

    //метод возвращает приватную переменную id документов
    std::vector<int> GetDocumentId() { return document_id_; };

    std::vector<int>::const_iterator begin() const;

    std::vector<int>::const_iterator end() const;

    std::vector<int>::iterator begin();

    std::vector<int>::iterator end();

    //метод удаляет документ
    void RemoveDocument(int document_id);

    //метод получения частот слов по id документа
    const std::map<std::string, double> &GetWordFrequencies(int document_id) const;

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
    //id документов
    std::vector<int> document_id_;
    //структура документов
    std::map<int, DocumentData> documents_;
    //структура сохраняющая стоп слова
    const std::set<std::string> stop_words_;
    //структура сохраняющая частоту слов в документе
    std::map<int, std::map<std::string, double>> word_freqs_;
    //структура которая сопоставляет каждому слову словарь «документ → TF»
    std::map<std::string, std::map<int, double>> word_to_document_freqs_;

    //метод проверки на стоп слово
    bool IsStopWord(const std::string &word) const;

    //метод проверки на валидность слов
    static bool IsValidWord(const std::string &word);

    //метод очищающий запрос от стоп слов
    std::vector<std::string> SplitIntoWordsNoStop(const std::string &text) const;

    //метод расчитывающий рейтинг слов
    static int ComputeAverageRating(const std::vector<int> &ratings);

    struct QueryWord {
        std::string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(const std::string &text) const;

    struct Query {
        std::set<std::string> plus_words;
        std::set<std::string> minus_words;
    };

    Query ParseQuery(const std::string &text) const;

    double ComputeWordInverseDocumentFreq(const std::string &word) const;

    //метод поиска всех докуметов
    template<typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const Query &query, DocumentPredicate document_predicate) const {
        std::map<int, double> document_to_relevance;
        for (const std::string &word: query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq]: word_to_document_freqs_.at(word)) {
                const auto &document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }
        for (const std::string &word: query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _]: word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }
        std::vector<Document> matched_documents;
        for (const auto [document_id, relevance]: document_to_relevance) {
            matched_documents.push_back(
                    {document_id, relevance, documents_.at(document_id).rating});
        }
        return matched_documents;
    }
};