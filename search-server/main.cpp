#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

struct Document {
    Document() = default;

    Document(int id, double relevance, int rating)
        : id(id)
        , relevance(relevance)
        , rating(rating) {
    }

    int id = 0;
    double relevance = 0.0;
    int rating = 0;
};

template <typename StringContainer>
set<string> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
    set<string> non_empty_strings;
    for (const string& str : strings) {
        if (!str.empty()) {
            non_empty_strings.insert(str);
        }
    }
    return non_empty_strings;
}

template<typename TypeStop>
set<string> SetStopWords(const TypeStop& stopwords) {
    set<string> stop_words;
    for (const string& word : stopwords) {
        stop_words.insert(word);
    }
    return stop_words;
}

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    inline static constexpr int INVALID_DOCUMENT_ID = -1;
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words)
        : stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {
            auto word = SetStopWords(stop_words);
            if (!all_of(word.begin(), word.end(), IsValidWord)) {
                throw invalid_argument("invalid document: unacceptable symbols"s);
            }
        }

    explicit SearchServer(const string& stop_words_text)
        : SearchServer(
            SplitIntoWords(stop_words_text))  // Invoke delegating constructor from string container
    {
    }
    
    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
        if (document_id < 0 ) {
            throw invalid_argument("invalid document: document id must be positive"s);
        }
        if (!IsValidWord(document)) {
            throw invalid_argument("invalid document: unacceptable symbols"s);
        }
        if (documents_.count(document_id)) {
            throw invalid_argument("invalid document: there is already a document with the given id"s);
        }
        document_id_.push_back(document_id);
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
    }

    template <typename DocumentPredicate>
    vector<Document> FindTopDocuments(const string& raw_query, DocumentPredicate document_predicate) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, document_predicate);
        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                 if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
                     return lhs.rating > rhs.rating;
                 }
                 return lhs.relevance > rhs.relevance;
             });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const {
        return FindTopDocuments(raw_query, [status](const int& document_id, DocumentStatus document_status, int rating){
            return document_status == status;
        });
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(raw_query, [](const int& document_id, DocumentStatus status, int rating){return status == DocumentStatus::ACTUAL;});
    }
    
    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
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
    
    int GetDocumentCount() const {
        return documents_.size();
    }
    
    int GetDocumentId(int index) const {
        if (index >= 0 && index <= document_id_.size()) {
            return document_id_[index];
        }
 
        throw out_of_range("invalid document id"s);
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
    const set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;
    vector<int> document_id_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }
    
    static bool IsValidWord(const string& word) {
        return none_of(word.begin(), word.end(), [](char c) {
            return c >= '\0' && c < ' ';
        });
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = 0;
        for (const int rating : ratings) {
            rating_sum += rating;
        }
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return {text, is_minus, IsStopWord(text)};
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
        Query query;
        if(!IsValidWord(text)) {
            throw invalid_argument("invalid document: unacceptable symbols"s);
        }
        for (const string& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                } else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
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
        return query;
    }

    // Existence required
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <typename DocumentPredicate>
    vector<Document> FindAllDocuments(const Query& query,
                                      DocumentPredicate document_predicate) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back(
                {document_id, relevance, documents_.at(document_id).rating});
        }
        return matched_documents;
    }
};

void PrintDocument(const Document& document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating << " }"s << endl;
}

int main() {
    cout << "Search server testing finished"s << endl;
    const vector<string> stop_words_vector = {"и"s, "в"s, "на"s, ""s, "в"s};
    SearchServer search_server(stop_words_vector);
	search_server.AddDocument(0, "белый кот и модный ошейник"s,
			DocumentStatus::ACTUAL, { 8, -3 });
	search_server.AddDocument(1, "пушистый кот пушистый хвост"s,
			DocumentStatus::ACTUAL, { 7, 2, 7 });
	search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s,
			DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
	search_server.AddDocument(3, "ухоженный скворец евгений"s,
			DocumentStatus::BANNED, { 9 });
	cout << "ACTUAL by default:"s << endl;

	for (const Document &document : search_server.FindTopDocuments(
			"пушистый ухоженный кот"s)) {
		PrintDocument(document);
	}
	cout << "BANNED:"s << endl;
	for (const Document &document : search_server.FindTopDocuments(
			"пушистый ухоженный кот"s, DocumentStatus::BANNED)) {
		PrintDocument(document);
	}
	cout << "Even ids:"s << endl;

	for (const Document &document : search_server.FindTopDocuments(
			"пушистый ухоженный кот"s,
			[](int document_id, DocumentStatus status, int rating) {
				return document_id % 2 == 0;
			})) {
		PrintDocument(document);
	}

}