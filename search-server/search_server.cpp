#include "search_server.h"

using namespace std;

SearchServer::SearchServer(const string& stop_words_text)
        : SearchServer(
        SplitIntoWords(stop_words_text))
{
}

void SearchServer::AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
    if (document_id < 0 ) {
        throw invalid_argument("invalid document: document id must be positive");
    }
    if (!IsValidWord(document)) {
        throw invalid_argument("invalid document: unacceptable symbols");
    }
    if (documents_.count(document_id)) {
        throw invalid_argument("invalid document: there is already a document with the given id");
    }
    document_id_.push_back(document_id);
    const vector<string> words = SplitIntoWordsNoStop(document);
    const double inv_word_count = 1.0 / words.size();
    for (const string word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
}

vector<Document> SearchServer::FindTopDocuments(const string& raw_query, DocumentStatus status) const {
    return FindTopDocuments(raw_query, [status](const int& document_id, DocumentStatus document_status, int rating){
        return document_status == status;
    });
}

vector<Document> SearchServer::FindTopDocuments(const string& raw_query) const {
    return FindTopDocuments(raw_query, [](const int& document_id, DocumentStatus status, int rating){return status == DocumentStatus::ACTUAL;});
}

tuple<vector<string>, DocumentStatus> SearchServer::MatchDocument(const string& raw_query, int document_id) const {
    if(!IsValidWord(raw_query)) {
        throw invalid_argument("invalid document: unacceptable symbols"s);
    }
    const Query query = ParseQuery(raw_query);
    for (const string& word : query.minus_words) {
        if (word[0] == '-'){
            throw invalid_argument("invalid document: no words"s);
        }
        if (word == "-" || !IsValidWord(word)){
            throw invalid_argument("invalid document: several characters '-' in a row"s);
        }
        if (word.size() == 0){
            throw invalid_argument("invalid document: word must not be empty"s);
        }
    }
    vector<string> matched_words;
    for (const string& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }
    for (const string& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.clear();
            break;
        }
    }

    return make_tuple(matched_words, documents_.at(document_id).status);
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

int SearchServer::GetDocumentId(int index) const {
    if (index >= 0 && index <= document_id_.size()) {
        return document_id_[index];
    }
    throw out_of_range("invalid document id");
}

bool SearchServer::IsStopWord(const string& word) const {
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(const string& word) {
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}

vector<string> SearchServer::SplitIntoWordsNoStop(const string& text) const {
    vector<string> words;
    for (const string& word : SplitIntoWords(text)) {
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    auto rating_sum = std::accumulate(ratings.begin(), ratings.end(), 0);
    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(const string& text) const {
    if (text.empty()) {
        throw invalid_argument("Query word is empty");
    }
    string word = text;
    bool is_minus = false;
    if (word[0] == '-') {
        is_minus = true;
        word = word.substr(1);
    }
    if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
        throw invalid_argument("Query word " + text + " is invalid");
    }

    return {word, is_minus, IsStopWord(word)};
}

SearchServer::Query SearchServer::ParseQuery(const std::string& text) const {
    Query query;
    if(!IsValidWord(text)) {
        throw std::invalid_argument("invalid document: unacceptable symbols"s);
    }
    for (const std::string& word : SplitIntoWords(text)) {
        const QueryWord query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                query.minus_words.insert(query_word.data);
            } else {
                query.plus_words.insert(query_word.data);
            }
        }
    }
    for (const std::string& word : query.minus_words) {
        if (word[0] == '-'){
            throw std::invalid_argument("invalid document: no words"s);
        }
        if (word == "-" || !IsValidWord(word)){
            throw std::invalid_argument("invalid document: several characters '-' in a row"s);
        }
        if (word.size() == 0){
            throw std::invalid_argument("invalid document: word must not be empty"s);
        }
    }
    return query;
}

double SearchServer::ComputeWordInverseDocumentFreq(const string& word) const {
    return std::log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}