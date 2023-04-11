#pragma once

#include <set>
#include <vector>
#include <string>
#include <iostream>

//метод разделяет слова по пробелам
std::vector <std::string_view> SplitIntoWords(std::string_view text);

//метод проверяет запрос на пустоту
template<typename StringContainer>
std::set <std::string, std::less<>> MakeUniqueNonEmptyStrings(const StringContainer &strings) {
    std::set <std::string, std::less<>> non_empty_strings;
    for (const auto &str: strings) {
        if (!str.size() == 0) {
            std::string my_str(str);
            non_empty_strings.emplace(my_str);
        }
    }
    return non_empty_strings;
}