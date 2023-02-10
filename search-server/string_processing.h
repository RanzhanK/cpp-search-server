#pragma once

#include <string>
#include <iostream>
#include <vector>
#include <set>

std::vector<std::string> SplitIntoWords(const std::string& text);

template <typename StringContainer>
std::set<std::string> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
    std::set<std::string> non_empty_strings;
    for (const std::string& str : strings) {
        if (!str.empty()) {
            non_empty_strings.insert(str);
        }
    }
    return non_empty_strings;
}

template<typename TypeStop>
std::set<std::string> SetStopWords(const TypeStop& stopwords) {
    std::set<std::string> stop_words;
    for (const std::string& word : stopwords) {
        stop_words.insert(word);
    }
    return stop_words;
}