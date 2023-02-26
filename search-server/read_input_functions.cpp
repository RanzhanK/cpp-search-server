#include "read_input_functions.h"

using namespace std;

//метод считывает слова
string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

//метод считывает цифры
int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}