#pragma once

#include <string>

namespace listki {

// The language preference is deliberately kept outside LISTKI01. This keeps
// the note/index format byte-for-byte compatible with older Listki releases.
enum class Language { Russian, English };
inline Language gLanguage = Language::Russian;

inline bool isEnglish() { return gLanguage == Language::English; }

inline const wchar_t* langText(const wchar_t* russian, const wchar_t* english) {
    return isEnglish() ? english : russian;
}

inline std::wstring langString(const wchar_t* russian, const wchar_t* english) {
    return langText(russian, english);
}

inline std::wstring languageName(Language language) {
    return language == Language::English ? L"English" : L"Русский";
}

inline const wchar_t* colorName(int color) {
    static const wchar_t* russian[] = {L"Ваниль", L"Шалфей", L"Небо", L"Сирень", L"Бумага"};
    static const wchar_t* english[] = {L"Vanilla", L"Sage", L"Sky", L"Lilac", L"Paper"};
    if (color < 0 || color >= 5) color = 0;
    return isEnglish() ? english[color] : russian[color];
}

inline std::wstring languageFileValue() {
    return isEnglish() ? L"en\r\n" : L"ru\r\n";
}

} // namespace listki
