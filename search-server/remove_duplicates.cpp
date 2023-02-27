#include <map>
#include <set>
#include <cmath>
#include <deque>
#include <string>
#include <vector>
#include <numeric>
#include <utility>
#include <iostream>
#include <stdexcept>
#include <algorithm>

#include "document.h"
#include "search_server.h"
#include "remove_duplicates.h"
#include "string_processing.h"

//метод поиска и удаления дубликатов
void RemoveDuplicates(SearchServer &search_server) {
    LOG_DURATION("RemoveDuplicates", std::cerr);

    std::set <std::set<std::string>> document_struct;
    std::set<int> duplicates;

    for (const int document_id: search_server) {
        std::set <std::string> key_words;
        std::map<std::string, double> words_freq_id = search_server.GetWordFrequencies(document_id);
        for (auto &&[first, second]: words_freq_id) {
            key_words.insert(first);
        }
        if (document_struct.count(key_words) == 0) {
            document_struct.insert(key_words);
        } else {
            std::cout << "Found duplicate document id " << document_id << std::endl;
            duplicates.insert(document_id);
        }
    }

    for (auto document_id: duplicates) {
        search_server.RemoveDocument(document_id);
    }
}