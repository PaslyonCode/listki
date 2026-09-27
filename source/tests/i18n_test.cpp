#include "../src/i18n.hpp"
#include <cassert>
#include <iostream>

using namespace listki;

int main() {
    gLanguage = Language::Russian;
    assert(std::wstring(langText(L"Русский", L"English")) == L"Русский");
    assert(std::wstring(colorName(0)) == L"Ваниль");
    gLanguage = Language::English;
    assert(std::wstring(langText(L"Русский", L"English")) == L"English");
    assert(std::wstring(colorName(0)) == L"Vanilla");
    assert(languageFileValue() == L"en\r\n");
    gLanguage = Language::Russian;
    assert(languageFileValue() == L"ru\r\n");
    std::cout << "PASS: language labels and preference values\n";
}
