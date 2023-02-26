#pragma once

#include <ostream>
#include <iostream>

//статусы документов
enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

//структура документов
struct Document {
    Document() = default;

    Document(int id, double relevance, int rating)
            : id(id), relevance(relevance), rating(rating) {
    }

    int id = 0;
    double relevance = 0.0;
    int rating = 0;
};

//метод вывода документов
std::ostream &operator<<(std::ostream &out, const Document &document);