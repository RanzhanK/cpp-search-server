#include "document.h"

//метод вывода документов
std::ostream &operator<<(std::ostream &out, const Document &document) {
    out << "{ "
        << "document_id = " << document.id << ", "
        << "relevance = " << document.relevance << ", "
        << "rating = " << document.rating << " }";
    return out;
}