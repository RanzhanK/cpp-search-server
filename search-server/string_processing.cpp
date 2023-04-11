#include "string_processing.h"

using namespace std;

//метод разделяет слова по пробелам
vector <string_view> SplitIntoWords(string_view text) {
    vector <string_view> result;
    while (true) {
        size_t space = text.find(' ');
        result.push_back(text.substr(0, space));
        if (space == text.npos) {
            break;
        } else {
            text.remove_prefix(space + 1);
        }
    }
    return result;
}