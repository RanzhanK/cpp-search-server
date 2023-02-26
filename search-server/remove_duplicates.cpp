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
    std::vector<std::pair<int, std::vector<std::string>>> document_struct;

    for (const int document_id: search_server.GetDocumentId()) {
        std::map<std::string, double> words_freq_id = search_server.GetWordFrequencies(document_id);
        std::vector<std::string> key_words;

        for (std::map<std::string, double>::iterator i = words_freq_id.begin(); i != words_freq_id.end(); ++i) {
            key_words.push_back(i->first);
        }

        document_struct.push_back(std::make_pair(document_id, key_words));
    }

    for (auto i = 0; i < (int) document_struct.size() - 1; ++i) {
        for (auto j = i + 1; j < (int) document_struct.size(); ++j) {
            if (document_struct[i].second == document_struct[j].second) {
                std::cout << "Found duplicate document id " << document_struct[j].first << std::endl;
                search_server.RemoveDocument(document_struct[j].first);
                document_struct.erase(document_struct.begin() + j);
                j--;
            }
        }
    }
}