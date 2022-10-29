#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <cstdlib>
#include <iomanip>

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
    int id;
    double relevance;
    int rating;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status,
                     const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(raw_query, [](const int& document_id, DocumentStatus status, int rating){return status == DocumentStatus::ACTUAL;});
    }

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const {
        return FindTopDocuments(raw_query, [status](const int& document_id, DocumentStatus document_status, int rating){
            return document_status == status;
        });
    }

    template <typename Predicate>
    vector<Document> FindTopDocuments(const string& raw_query, Predicate predicate) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, predicate);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
                    return lhs.rating > rhs.rating;
                } else {
                    return lhs.relevance > rhs.relevance;
                }
             });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

    int GetDocumentCount() const {
        return documents_.size();
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query,
                                                        int document_id) const {
        const Query query = ParseQuery(raw_query);
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
        return {matched_words, documents_.at(document_id).status};
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
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
        return query;
    }

    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <typename Predicate>
    vector<Document> FindAllDocuments(const Query& query, Predicate predicate) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                if (predicate(document_id, documents_.at(document_id).status, documents_.at(document_id).rating)) {
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


template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
                     const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cout << boolalpha;
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cout << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
                const string& hint) {
    if (!value) {
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename functions>
void RunTestImpl(const functions& func, const string& func_name) {
    func();
    cerr << func_name << " OK"s << endl;
}

#define RUN_TEST(func) RunTestImpl((func), #func)

// Тест проверяет, что документы добавляются
void TestAddedDocument () {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    // Сначала убеждаемся, что документов нет
    {
        SearchServer server;
        ASSERT(server.FindTopDocuments("in"s).empty());
    }
    // Затем убеждаемся, что документ добавлен
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("cat"s);
        ASSERT_EQUAL(found_docs.size(), 1);
    }
    // Затем убеждаемся, слова которых в документе нет
    // возвращает пустой результат
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("dog"s);
        ASSERT(found_docs.empty());
    }
    // Проверяем добавление документа из пробелов
    {
        SearchServer server;
        server.AddDocument(doc_id,  "   ", DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("   dog"s);
        ASSERT(found_docs.empty());
    }
    // Проверяем добавление пустого документа
    {
        SearchServer server;
        server.AddDocument(doc_id,  "", DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("dog"s);
        ASSERT(found_docs.empty());
    }
}
// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    // Сначала убеждаемся, что поиск слова, не входящего в список стоп-слов,
    // находит нужный документ
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }
    // Затем убеждаемся, что поиск этого же слова, входящего в список стоп-слов,
    // возвращает пустой результат
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
                    "Stop words must be excluded from documents"s);
    }
}
// Тест проверяет, что поисковая система исключает минус-слова при добавлении документов
void TestExcludeMinusWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    // Сначала убеждаемся, что поиск слова, не входящего в минус слова,
    // находит нужный документ
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("cat"s);
        ASSERT_EQUAL(found_docs.size(), 1);
    }
    // Убеждаемся, что поиск минус слова, возвращает пустой результат
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("in the -city"s).empty());
    }
}
// Тест проверяет, матчинг документов
void TestMatchDocument() {
    const int doc_id = 42;
    const string content = "cat in the Moscow city"s;
    const vector<int> ratings = {1, 2, 3};
    // При матчинге документа по поисковому запросу должны быть возвращены
    // все слова из поискового запроса
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        auto [words, document_id] = server.MatchDocument("cats in the Moscow city"s, doc_id);
        ASSERT_EQUAL(words.size(), 4);
    }
    // При соответствие хотя бы по одному минус-слову, должен возвращаться
    // пустой список слов
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        auto [words, document_id] = server.MatchDocument("cats in the -city Moscow"s, doc_id);
        ASSERT(words.empty());
    }
}
// Тест проверяет, cортировку найденных документов по релевантности
void TestRelevanceSorting() {
	const int doc_id_1 = 1;
	const string content_1 = "cat in the Moscow city"s;
	const vector<int> ratings_1 = {1, 2, 3};
	const int doc_id_2 = 2;
	const string content_2 = "cat in the big city"s;
	const vector<int> ratings_2 = {4, 5, 6};
	const int doc_id_3 = 3;
	const string content_3 = "big cat in the big city"s;
	const vector<int> ratings_3 = {2, 4, 2};
    // Сначала убеждаемся, что релевантность отсортирована по убыванию
    {
        SearchServer server;
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
        server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings_2);
        server.AddDocument(doc_id_3, content_3, DocumentStatus::ACTUAL, ratings_3);
        const auto found_docs = server.FindTopDocuments("cat big city"s);
        ASSERT_EQUAL(found_docs.size(), 3);
        ASSERT_EQUAL(found_docs[0].id, doc_id_3);
        ASSERT_EQUAL(found_docs[1].id, doc_id_2);
        ASSERT_EQUAL(found_docs[2].id, doc_id_1);
    }
    // Убеждаемся, что релевантность отсортирована по возрастанию
    {
        SearchServer server;
        server.AddDocument(doc_id_1, content_3, DocumentStatus::ACTUAL, ratings_1);
        server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings_2);
        server.AddDocument(doc_id_3, content_1, DocumentStatus::ACTUAL, ratings_3);
        const auto found_docs = server.FindTopDocuments("cat big city"s);
        ASSERT_EQUAL(found_docs.size(), 3);
        ASSERT_EQUAL(found_docs[0].id, doc_id_1);
        ASSERT_EQUAL(found_docs[1].id, doc_id_2);
        ASSERT_EQUAL(found_docs[2].id, doc_id_3);
    }
}
// Тест вычисляет рейтинг документов.
void TestRating() {
    const int doc_id = 42;
    const string content = "cat in the big Moscow city"s;
    const vector<int> ratings = {1, 2, 3};
    // Сначала убеждаемся, что релевантность отсортирована по убыванию
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("funny cute cat"s);
        ASSERT_EQUAL(found_docs[0].rating, ((1+2+3+0.0)/3));
    }
}
// Тест проверяет фильтрацию результатов поиска с использованием
// предиката, задаваемого пользователем.
void TestDocumentPredicate() {
    const int doc_id_1 = 0;
	const string content_1 = "cat in the Moscow"s;
	const vector<int> ratings_1 = {8, -3};
	const int doc_id_2 = 1;
	const string content_2 = "cat in the big city"s;
	const vector<int> ratings_2 = {7, 2, 7};
	const int doc_id_3 = 2;
	const string content_3 = "big cat in the big city"s;
	const vector<int> ratings_3 = {5, -12, 2, 1};
    // Убеждаемся что выдан нужный документ
    {
        SearchServer server;
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
        server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings_2);
        server.AddDocument(doc_id_3, content_3, DocumentStatus::ACTUAL, ratings_3);
        const auto found_docs = server.FindTopDocuments("cat in big city"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; });
        ASSERT_EQUAL(found_docs.size(), 2);
        ASSERT_EQUAL(found_docs[0].id, doc_id_3);
        ASSERT_EQUAL(found_docs[1].id, doc_id_1);
    }

    {
        SearchServer server;
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
        server.AddDocument(doc_id_2, content_2, DocumentStatus::REMOVED, ratings_2);
        server.AddDocument(doc_id_3, content_3, DocumentStatus::ACTUAL, ratings_3);
        const auto found_docs = server.FindTopDocuments("cat in big city"s, [](int document_id, DocumentStatus status, int rating) {return status == DocumentStatus::REMOVED;});
        ASSERT_EQUAL(found_docs[0].id, doc_id_2);
    }
}
// Поиск документов, имеющих заданный статус
void TestStatus() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    const DocumentStatus document_status = DocumentStatus::BANNED;
    // Проверяем, что документы с подходящими словами не выдаются с не актуальным статусом
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, document_status, ratings);
        const auto found_docs = server.FindTopDocuments("beautiful cat in the town"s, DocumentStatus::ACTUAL);
        ASSERT(found_docs.empty());
    }
    // Проверяем, что документы с подходящими словами и подходящим статусом выдаются
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::BANNED, ratings);
        const auto found_docs = server.FindTopDocuments("beautiful cat in the town"s, DocumentStatus::BANNED);
        ASSERT(!found_docs.empty());
        ASSERT_EQUAL(found_docs[0].id, doc_id);
    }
}
// Корректное вычисление релевантности найденных документов.
void TestRelevance() {
	const int doc_id_1 = 0;
	const string content_1 = "cat in the Moscow city"s;
	const vector<int> ratings_1 = {8, -3};
	const int doc_id_2 = 1;
	const string content_2 = "dog in the Moscow city"s;
	const vector<int> ratings_2 = {7, 2, 7};
	const int doc_id_3 = 2;
	const string content_3 = "bat in the Moscow city"s;
	const vector<int> ratings_3 = {5, -12, 2, 1};
	//const double relevance = 0.219722;
    // Убеждаемся, что релевантность отсортирована по убыванию
    {
        SearchServer server;
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
        server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings_2);
        server.AddDocument(doc_id_3, content_3, DocumentStatus::ACTUAL, ratings_3);
        const auto found_docs = server.FindTopDocuments("cat"s);
        ASSERT_EQUAL(found_docs.size(),  1);
    }
}
// Корректное вычисление количества найденных документов.
void TestGetDocumentCount() {
	const int doc_id_1 = 0;
	const string content_1 = "cat in the Moscow city"s;
	const vector<int> ratings_1 = {8, -3};
	const int doc_id_2 = 1;
	const string content_2 = "dog in the Moscow city"s;
	const vector<int> ratings_2 = {7, 2, 7};
	const int doc_id_3 = 2;
	const string content_3 = "bat in the Moscow city"s;
	const vector<int> ratings_3 = {5, -12, 2, 1};
	// Убеждаемся, что выдает верное количество документов
	{
		SearchServer server;
	    server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
	    server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings_2);
	    server.AddDocument(doc_id_3, content_3, DocumentStatus::ACTUAL, ratings_3);
	    ASSERT_EQUAL(server.GetDocumentCount(), 3);
	}

}
void TestDocumentRelevanceCalc() {
	SearchServer server;
	const int doc_id_1 = 0;
	const vector<int> ratings_1 = {1};
	string content_1 = "пушистый ухоженный кот"s;
	server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);

	const int doc_id_2 = 1;
	const string content_2 = "пушистый ухоженный пес"s;
	const vector<int> ratings_2 = {2};
	server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings_2);

	const auto found_docs = server.FindTopDocuments("кот"s);
	ASSERT_EQUAL(found_docs[0].relevance, 0.23104906018664842);
}
// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestExcludeMinusWordsFromAddedDocumentContent);
    RUN_TEST(TestDocumentRelevanceCalc);
    RUN_TEST(TestDocumentPredicate);
    RUN_TEST(TestRelevanceSorting);
    RUN_TEST(TestGetDocumentCount);
    RUN_TEST(TestAddedDocument);
    RUN_TEST(TestMatchDocument);
    RUN_TEST(TestRelevance);
    RUN_TEST(TestRating);
    RUN_TEST(TestStatus);
}
// --------- Окончание модульных тестов поисковой системы -----------

int main() {
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;
}
