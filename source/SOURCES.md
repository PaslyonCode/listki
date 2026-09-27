# Исходный код и проверка

Язык: C++17. Интерфейс: Win32, системный RichEdit (Msftedit.dll), GDI/GDI+ и Common Controls. Сторонние библиотеки и сетевые API не используются. C++ runtime статически включён в EXE.

## Сборка

Windows: установите MinGW-w64 для x86-64, добавьте его `bin` в PATH и выполните `build.bat`.

Linux: нужны `g++-mingw-w64-x86-64-posix` и `binutils-mingw-w64-x86-64`. Выполните:

```bash
bash build.sh
```

Результат: `build/Listki.exe`.

## Проверенные тесты модели

```bash
g++ -std=c++17 -O2 tests/model_test.cpp -o model_test
./model_test
g++ -std=c++17 -O2 tests/note_ui_test.cpp -o note_ui_test
./note_ui_test
g++ -std=c++17 -O2 tests/i18n_test.cpp -o i18n_test
./i18n_test
```

Проверяются постоянное название «Новая записка», сохранение ручных названий, Unicode и многострочные поля, отрицательные координаты второго монитора, сортировка и ограничение истории десятью записками, сохранение записок при удалении блокнота, повреждённые и обрезанные файлы, неверные идентификаторы и ссылка на отсутствующий блокнот. Отдельно проверяются геометрия кнопок и всплывающей панели при масштабе 100–250%, русские и английские подписи, переключение языка и хранение предпочтения вне индекса.

## Интеграционный тест Windows

Тест скомпилирован, но не выполнен в среде подготовки этой сборки. Он должен запускаться в отдельной пустой тестовой папке; не указывайте папку с настоящими записками.

Из MinGW-w64:

```bat
g++ -std=c++17 -O2 -municode -static tests\windows_test.cpp build\resources.o -o windows_test.exe -lcomctl32 -lcomdlg32 -lgdiplus -lgdi32 -lshell32 -lole32 -luuid -ladvapi32 -luser32
windows_test.exe C:\Temp\Listki-test-empty
```

Тест создаёт окна, работает с RichEdit и временно заменяет содержимое буфера обмена. Он проверяет создание записки, встроенное поле заголовка (автосохранение, Enter, Esc, возврат к названию по умолчанию), сохранность команд и подменю при смене оформления, постоянный заголовок, появление панели форматирования, сохранение выделения при её использовании, Unicode, форматирование выделения и всего текста, закрепление, геометрию, скрытие/открытие, вставку из буфера, списки, поиск, блокноты, расположение кнопок около углов, перезагрузку файлов, неудачную запись и резервное восстановление. Сообщение `ALL ... WINDOWS INTEGRATION CHECKS PASSED` появляется только после успешного прохождения всех проверок.

Проверки через пользовательский интерфейс перед выпуском следующей версии: автозапуск после входа в Windows; запуск второй копии; трей после перезапуска Explorer; масштаб 100/150/200%; перенос между мониторами; Ctrl+V вставляет текст ровно один раз; повторный запуск восстанавливает окна, шрифты и блокноты.

## Файлы

- `src/app.cpp`: окна, меню, трей, редактирование и жизненный цикл.
- `src/platform.hpp`: Win32, рисование, Unicode и атомарная запись файлов.
- `src/modern.hpp`: единое оформление меню и модальных окон.
- `src/font_dialog.hpp`: оформление системного диалога шрифта с сохранением его функций.
- `src/model.hpp`: формат индекса и независимая модель данных.
- `src/i18n.hpp`: переключаемые русские и английские подписи; язык хранится отдельно от `LISTKI01`.
- `src/note_ui.hpp`: геометрия поля заголовка, кнопок, списков и панели форматирования, используемые кодом интерфейса и тестами.
- `src/Listki.manifest`: запуск без повышения прав и DPI awareness.
- `src/Listki.rc`, `src/Listki.ico`: ресурсы и значок.
- `tests/`: тесты модели, интерфейсной геометрии, языка и интеграции Windows.

В автозапуск записывается только значение текущего пользователя `HKCU\Software\Microsoft\Windows\CurrentVersion\Run\Listki`, по явному включению пользователем. Другие параметры Windows программа не меняет.

Техническая документация Microsoft, использованная при реализации:

- https://learn.microsoft.com/en-us/windows/win32/controls/em-streamout
- https://learn.microsoft.com/en-us/windows/win32/controls/em-streamin
- https://learn.microsoft.com/en-us/windows/win32/controls/em-setcharformat
- https://learn.microsoft.com/en-us/windows/win32/controls/em-settextmode
- https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setlayeredwindowattributes
- https://learn.microsoft.com/en-us/windows/win32/controls/en-selchange
- https://learn.microsoft.com/en-us/windows/win32/controls/em-posfromchar
- https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-mouseactivate

- https://learn.microsoft.com/en-us/windows/win32/controls/em-setcuebanner
- https://learn.microsoft.com/en-us/windows/win32/menurc/using-menus
- https://learn.microsoft.com/en-us/windows/win32/api/commdlg/ns-commdlg-choosefontw
