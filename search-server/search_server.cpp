#include "search_server.h"

using namespace std;

SearchServer::SearchServer(string_view stop_words_text)
        : SearchServer(
        SplitIntoWords(stop_words_text)){
}

SearchServer::SearchServer(const string& stop_words_text)
        : SearchServer(
        SplitIntoWords(stop_words_text)){
}

//метод добавления документов
void SearchServer::AddDocument(int document_id, string_view document, DocumentStatus status, const vector<int> &ratings) {
    if ((document_id < 0) || (documents_.count(document_id) > 0)) {
        throw std::invalid_argument("Invalid document_id");
    }
    deque_document_structure_.push_back(string(document));
    const auto words = SplitIntoWordsNoStop(string_view(deque_document_structure_.back()));
    const double inv_word_count = 1.0 / words.size();
    for (const auto& word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
        word_freqs_[document_id][word] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
    document_id_.insert(document_id);
}

//метод поиска топ докуметов с заданным статусом
vector<Document> SearchServer::FindTopDocuments(string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(raw_query, [status](const int &document_id, DocumentStatus document_status, int rating) {
        return document_status == status;
    });
}
//метод поиска топ докуметов с актуальным статусом
vector<Document> SearchServer::FindTopDocuments(string_view raw_query) const {
    return FindTopDocuments(raw_query, [](const int &document_id, DocumentStatus status, int rating) {
        return status == DocumentStatus::ACTUAL;
    });
}

//метод возвращает все плюс-слова запроса, содержащиеся в документе отсортированые по возрастанию.
//если нет пересечений по плюс-словам или есть минус-слово, вектор слов возвращается пустым.
tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(string_view raw_query, int document_id) const {
    const auto query = ParseQuery(raw_query);
    vector<string_view> matched_words;
    for (const string_view word: query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            return { matched_words, documents_.at(document_id).status };
        }
    }
    for (const string_view word: query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }

    return { matched_words, documents_.at(document_id).status };
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(const execution::sequenced_policy&, string_view raw_query, int document_id) const {
    return SearchServer::MatchDocument(raw_query, document_id);
}

//паралельный метод возвращает все плюс-слова запроса, содержащиеся в документе отсортированые по возрастанию.
tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(const execution::parallel_policy& policy, string_view raw_query, int document_id) const {
    //добавляем флаг true, что бы вызвался паралельный метод ParseQuery
    const auto query = ParseQuery(true, raw_query);
    //создаем вектор, с заранее подготовленным размером
    vector<string_view> matched_words;

    //проверка на совпадение по минус словам, возвращаем ноль, так как нет слов
    if(any_of(policy, query.minus_words.begin(), query.minus_words.end(), [&](auto& word){
        if (word_to_document_freqs_.count(word)){
            if (word_to_document_freqs_.at(word).count(document_id)) {
                return true;
            }
        }
        return false;
    }) == true) return {matched_words, documents_.at(document_id).status};

    //находим плюс слов
    matched_words.resize(query.plus_words.size());
    matched_words.clear();
    auto end = copy_if(policy, query.plus_words.begin(), query.plus_words.end(), matched_words.begin(), [&](auto raw_word){
        if (word_to_document_freqs_.count(raw_word) == 0) {
            return false;
        }
        if (word_to_document_freqs_.at(raw_word).count(document_id)) {
            return true;
        }
        return false;
    });

    //сортируем вектор для метода unique
    sort(matched_words.begin(), end);
    //ищем все последовательно повторяющиеся плюс слова из диапазона query
    auto it = unique(matched_words.begin(), end);
    //удаляем последовательно повторяющиеся плюс слова из диапазона query
    matched_words.erase(it, matched_words.end());

    return { matched_words, documents_.at(document_id).status };
}

//метод возвращает количество документов в поисковой системе.
int SearchServer::GetDocumentCount() const {
    return static_cast<int>(documents_.size());
}

set<int>::const_iterator SearchServer::begin() const {
    return document_id_.begin();
}

set<int>::const_iterator SearchServer::end() const {
    return document_id_.end();
}

set<int>::iterator SearchServer::begin() {
    return document_id_.begin();
}

set<int>::iterator SearchServer::end() {
    return document_id_.end();
}

//метод проверки на стоп слово
bool SearchServer::IsStopWord(string_view word) const {
    return stop_words_.count(word) > 0;
}

//метод проверки на валидность слов
bool SearchServer::IsValidWord(string_view word) {
    return std::none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}

//метод очищающий запрос от стоп слов
vector<string_view> SearchServer::SplitIntoWordsNoStop(string_view text) const {
    vector<string_view> words;
    for (string_view word: SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw invalid_argument("Word " + string(word) + " is invalid");
        }
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

//метод расчитывающий рейтинг слов
int SearchServer::ComputeAverageRating(const vector<int> &ratings) {
    if (ratings.empty()) {
        return 0;
    }
    auto rating_sum = std::accumulate(ratings.begin(), ratings.end(), 0);
    return rating_sum / static_cast<int>(ratings.size());
}

//метод позволяет опредлелить где минус а где плюс слова
SearchServer::QueryWord SearchServer::ParseQueryWord(std::string_view text) const {
    if (text.size()==0) {
        throw std::invalid_argument("Query word is empty");
    }
    bool is_minus = false;
    if (text[0] == '-') {
        is_minus = true;
        text = text.substr(1);
    }
    if (text.size()==0 || text[0] == '-' || !IsValidWord(text)) {
        throw std::invalid_argument("Query word " + std::string(text) + " is invalid");
    }

    return { text, is_minus, IsStopWord(text) };
}

SearchServer::Query SearchServer::ParseQuery(string_view text) const {
    Query query;
    //итерируемся по отдельно сформированным словам
    for (std::string_view word: SplitIntoWords(text)) {
        //определяем слова на плюс и минус слова
        auto query_word = ParseQueryWord(word);
        //если в запросе не стоп слова, то разделям минус и плюс слова
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                query.minus_words.push_back(query_word.data);
            } else {
                query.plus_words.push_back(query_word.data);
            }
        }
    }
    //сортируем вектор для метода unique
    sort(query.minus_words.begin(), query.minus_words.end());
    //ищем все последовательно повторяющиеся минус слова из диапазона query
    auto last_m = unique(query.minus_words.begin(), query.minus_words.end());
    //удаляем последовательно повторяющиеся минус слова из диапазона query
    query.minus_words.erase(last_m, query.minus_words.end());
    //сортируем вектор для метода unique
    sort(query.plus_words.begin(), query.plus_words.end());
    //ищем все последовательно повторяющиеся плюс слова из диапазона query
    auto last_p = unique(query.plus_words.begin(), query.plus_words.end());
    //удаляем последовательно повторяющиеся плюс слова из диапазона query
    query.plus_words.erase(last_p, query.plus_words.end());

    return query;
}

SearchServer::Query SearchServer::ParseQuery(const execution::sequenced_policy&, string_view text) const {
    return SearchServer::ParseQuery(text);
}

//паралельный метод парсинга
SearchServer::Query SearchServer::ParseQuery(bool flag, string_view text) const {
    Query query;
    //итерируемся по отдельно сформированным словам
    for (string_view word : SplitIntoWords(text)) {
        //определяем слова на плюс и минус слова
        const auto query_word = ParseQueryWord(word);
        //если в запросе не стоп слова, то разделям минус и плюс слова
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                query.minus_words.push_back(query_word.data);
            }
            else {
                query.plus_words.push_back(query_word.data);
            }
        }
    }
    return query;
}

double SearchServer::ComputeWordInverseDocumentFreq(const string &word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

//метод получения частот слов по id документа
const map<string_view, double> &SearchServer::GetWordFrequencies(int document_id) const {
    static map<string_view, double> word_freqs;
    if (word_freqs_.count(document_id) == 0) {
        return word_freqs;
    }
    return word_freqs_.at(document_id);
}

//метод удаления документов из поискового сервера
void SearchServer::RemoveDocument(int document_id) {
    documents_.erase(document_id);
    document_id_.erase(document_id);
    word_freqs_.erase(document_id);
    for (auto& [key, value] : word_to_document_freqs_) {
        value.erase(document_id);
    }
}

//однопоточный метод удаления документов из поискового сервера
void SearchServer::RemoveDocument(const execution::sequenced_policy&, int document_id) {
    vector<string> marks(word_freqs_.at(document_id).size());
    transform(execution::seq, word_freqs_.at(document_id).begin(), word_freqs_.at(document_id).end(), marks.begin(), [](auto& it) {
        return it.first;
    });

    auto p = [this, document_id](auto it) {
        word_to_document_freqs_.at(it).erase(document_id);
    };

    document_id_.erase(document_id);
    documents_.erase(document_id);
    word_freqs_.erase(document_id);
    for_each(std::execution::seq, marks.begin(), marks.end(), p);
}

//паралельный метод удаления документов из поискового сервера
void SearchServer::RemoveDocument(const execution::parallel_policy&, int document_id) {
    vector<string*> marks(word_freqs_.at(document_id).size(), nullptr);
    transform(execution::par, word_freqs_.at(document_id).begin(), word_freqs_.at(document_id).end(), marks.begin(), [](auto& it) {
        return new string(it.first);
    });

    auto p = [this, document_id](auto it) {
        word_to_document_freqs_.at(*it).erase(document_id);
    };

    document_id_.erase(document_id);
    documents_.erase(document_id);
    word_freqs_.erase(document_id);
    for_each(std::execution::par, marks.begin(), marks.end(), p);
}