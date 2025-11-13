# cterm - Console TUI Mail Client

**cterm** - легковесный консольный почтовый клиент с текстовым интерфейсом (TUI) для Linux.

## Особенности

- **Два интерфейса**: ncurses TUI для терминала и GTK+ GUI для графического окружения
- Поддержка IMAP для получения писем
- Поддержка SMTP для отправки писем
- **Поддержка вложений** - просмотр и сохранение файлов
- **Фильтрация и поиск писем** - по теме, отправителю, непрочитанным
- **Работа с несколькими папками** - INBOX, Sent, Drafts и другие
- **Адресная книга** - управление контактами
- **HTML рендеринг** - упрощенное отображение HTML писем
- SSL/TLS шифрование (OpenSSL)
- Минимальные зависимости
- Оптимизирован для ARM/встраиваемых систем

## Требования

### Библиотеки

- libc (стандартная библиотека C)
- ncurses (для TUI интерфейса)
- OpenSSL (для SSL/TLS поддержки)
- GTK+ 3.0 (опционально, для GUI версии)

### Установка зависимостей

**Debian/Ubuntu:**
```bash
# Минимальные (для ncurses версии)
sudo apt-get install build-essential libncurses-dev libssl-dev

# С поддержкой GUI
sudo apt-get install build-essential libncurses-dev libssl-dev libgtk-3-dev
```

**Fedora/RHEL:**
```bash
# Минимальные
sudo dnf install gcc make ncurses-devel openssl-devel

# С поддержкой GUI
sudo dnf install gcc make ncurses-devel openssl-devel gtk3-devel
```

**Arch Linux:**
```bash
# Минимальные
sudo pacman -S base-devel ncurses openssl

# С поддержкой GUI
sudo pacman -S base-devel ncurses openssl gtk3
```

## Сборка

### Вариант 1: Using Makefile (рекомендуется)

```bash
# Собрать ncurses версию (по умолчанию)
make

# Собрать только GTK+ GUI версию
make gui

# Собрать обе версии
make both
```

Для debug сборки:
```bash
make debug
```

Для release сборки:
```bash
make release
```

### Вариант 2: Using CMake

```bash
mkdir build
cd build
cmake ..
make
```

## Установка

```bash
sudo make install
```

Это установит `cterm` в `/usr/local/bin/`.

Для удаления:
```bash
sudo make uninstall
```

## Конфигурация

1. Скопируйте пример конфигурации:
```bash
cp config/cterm.conf.example ~/.cterm.conf
```

2. Отредактируйте `~/.cterm.conf` своими настройками:
```bash
nano ~/.cterm.conf
```

3. Укажите параметры вашего почтового сервера:
   - IMAP сервер для получения писем
   - SMTP сервер для отправки писем
   - Имя пользователя и пароль

### Пример для Gmail

Для Gmail используйте App Password (Пароль приложения):
1. Перейдите в Google Account Settings
2. Security → 2-Step Verification
3. App passwords → Generate new
4. Используйте сгенерированный пароль в конфиге

```
imap_server = imap.gmail.com
imap_port = 993
imap_use_ssl = yes
smtp_server = smtp.gmail.com
smtp_port = 587
smtp_use_starttls = yes
```

## Использование

### Запуск ncurses версии (терминал):
```bash
cterm
```

### Запуск GTK+ GUI версии:
```bash
cterm-gui
```

Или укажите альтернативный конфиг:
```bash
cterm -c /path/to/config.conf
cterm-gui -c /path/to/config.conf
```

### Горячие клавиши (ncurses)

**В списке писем:**
- `↑/↓` или `j/k` - навигация по письмам
- `Enter` - открыть письмо
- `C` - создать новое письмо
- `D` - удалить письмо
- `R` - обновить список писем
- `S` - поиск по письмам
- `F` - выбрать папку
- `A` - адресная книга
- `Q` - выход

**При просмотре письма:**
- `Esc` или `q` - вернуться к списку
- `D` - удалить письмо
- `M` - отметить как непрочитанное
- `A` - сохранить вложения (в /tmp/)

**При создании письма:**
- Заполните поля To, Subject, Body
- `F2` - отправить письмо
- `Esc` - отменить

### Функции GUI версии

- Графический интерфейс с деревом папок
- Список писем с фильтрацией
- Просмотр писем с HTML рендерингом
- Диалоги для создания писем, поиска
- Интеграция с адресной книгой
- Сохранение вложений через GUI

## Архитектура

```
cterm/
├── src/              # Исходные файлы
│   ├── main.c        # Точка входа
│   ├── config.c      # Парсинг конфигурации
│   ├── network.c     # Сетевые соединения + SSL/TLS
│   ├── imap.c        # IMAP протокол
│   ├── smtp.c        # SMTP протокол
│   └── ui.c          # ncurses TUI
├── include/          # Заголовочные файлы
├── config/           # Примеры конфигурации
└── docs/             # Документация
```

### Взаимодействие модулей

```
main.c
  ├─> config.c      (загрузка настроек)
  ├─> network.c     (TCP + SSL/TLS соединения)
  ├─> imap.c        (получение писем)
  │     └─> network.c
  ├─> smtp.c        (отправка писем)
  │     └─> network.c
  └─> ui.c          (TUI интерфейс)
        ├─> imap.c
        └─> smtp.c
```

## Поддерживаемые серверы

- Gmail (imap.gmail.com / smtp.gmail.com)
- Outlook/Hotmail (outlook.office365.com)
- Yahoo Mail (imap.mail.yahoo.com / smtp.mail.yahoo.com)
- Yandex (imap.yandex.ru / smtp.yandex.ru)
- Mail.ru (imap.mail.ru / smtp.mail.ru)
- Любые стандартные IMAP/SMTP серверы

## Улучшения в версии 2.0

Все функции из раздела "Дальнейшее развитие" были реализованы:
- ✅ Поддержка вложений
- ✅ Фильтрация писем
- ✅ Поиск по письмам
- ✅ Работа с несколькими папками
- ✅ Адресная книга
- ✅ HTML рендеринг (упрощенный)
- ✅ GTK+ GUI интерфейс

## Безопасность

- Пароли хранятся в открытом виде в `~/.cterm.conf`
- Рекомендуется установить права доступа:
  ```bash
  chmod 600 ~/.cterm.conf
  ```
- Используйте App Passwords для Gmail и других сервисов
- Все соединения шифруются через SSL/TLS

## Устранение неполадок

**Ошибка подключения к IMAP/SMTP:**
- Проверьте адреса серверов и порты
- Убедитесь, что используете правильный режим SSL/TLS
- Проверьте логин и пароль

**Gmail App Password:**
- Убедитесь, что двухфакторная аутентификация включена
- Создайте App Password в настройках безопасности

**Ошибки компиляции:**
- Убедитесь, что установлены все зависимости
- Проверьте версию GCC (требуется C99+)

## Лицензия

MIT License

## Автор

Проект создан как пример легковесного почтового клиента для Linux.

## Архитектура новых модулей

```
cterm/
├── src/
│   ├── addressbook.c     # Управление адресной книгой
│   ├── html_parser.c     # Конвертация HTML в текст
│   ├── gui.c             # GTK+ GUI интерфейс
│   └── main_gui.c        # Точка входа для GUI версии
├── include/
│   ├── addressbook.h
│   ├── html_parser.h
│   └── gui.h
```

### Новые возможности IMAP модуля:
- `imap_list_mailboxes()` - получение списка папок
- `imap_search_emails()` - поиск по IMAP SEARCH
- `imap_filter_by_subject/sender()` - фильтрация
- `imap_get_attachments()` - получение информации о вложениях
- `imap_fetch_attachment()` - скачивание вложений
