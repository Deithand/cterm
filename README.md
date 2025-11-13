# cterm - Console TUI Mail Client

**cterm** - легковесный почтовый клиент для Linux с двумя интерфейсами: текстовым (TUI) и графическим (GUI).

## Особенности

- **Два варианта интерфейса:**
  - `cterm` - терминальный интерфейс (ncurses TUI)
  - `cterm-gui` - графический интерфейс (GTK+3 GUI)
- Поддержка IMAP для получения писем
- Поддержка SMTP для отправки писем
- SSL/TLS шифрование (OpenSSL)
- Минимальные зависимости
- Оптимизирован для ARM/встраиваемых систем

## Требования

### Библиотеки

**Для TUI версии (cterm):**
- libc (стандартная библиотека C)
- ncurses (для TUI интерфейса)
- OpenSSL (для SSL/TLS поддержки)

**Для GUI версии (cterm-gui):**
- Все библиотеки из TUI версии
- GTK+3 (для графического интерфейса)

### Установка зависимостей

**Debian/Ubuntu:**
```bash
# Для TUI версии
sudo apt-get install build-essential libncurses-dev libssl-dev

# Дополнительно для GUI версии
sudo apt-get install libgtk-3-dev
```

**Fedora/RHEL:**
```bash
# Для TUI версии
sudo dnf install gcc make ncurses-devel openssl-devel

# Дополнительно для GUI версии
sudo dnf install gtk3-devel
```

**Arch Linux:**
```bash
# Для TUI версии
sudo pacman -S base-devel ncurses openssl

# Дополнительно для GUI версии
sudo pacman -S gtk3
```

## Сборка

### Вариант 1: Using Makefile (рекомендуется)

**Собрать обе версии (TUI и GUI):**
```bash
make
```

**Собрать только TUI версию:**
```bash
make tui
```

**Собрать только GUI версию:**
```bash
make gui
```

**Debug сборка:**
```bash
make debug
```

**Release сборка:**
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

### TUI версия (терминальный интерфейс)

Запустите программу:
```bash
cterm
```

Или укажите альтернативный конфиг:
```bash
cterm -c /path/to/config.conf
```

### GUI версия (графический интерфейс)

Запустите программу:
```bash
cterm-gui
```

Или укажите альтернативный конфиг:
```bash
cterm-gui -c /path/to/config.conf
```

**Особенности GUI версии:**
- Графическое окно с интуитивным интерфейсом
- Список писем в виде таблицы
- Панель предпросмотра письма
- Диалог композиции письма
- Кнопки управления: Refresh, Compose, Delete
- Строка состояния для отображения операций

### Горячие клавиши

**В списке писем:**
- `↑/↓` или `j/k` - навигация по письмам
- `Enter` - открыть письмо
- `C` - создать новое письмо
- `D` - удалить письмо
- `R` - обновить список писем
- `Q` - выход

**При просмотре письма:**
- `Esc` или `q` - вернуться к списку
- `D` - удалить письмо
- `M` - отметить как непрочитанное

**При создании письма:**
- Заполните поля To, Subject, Body
- `F2` - отправить письмо
- `Esc` - отменить

## Архитектура

```
cterm/
├── src/              # Исходные файлы
│   ├── main.c        # Точка входа TUI
│   ├── main_gui.c    # Точка входа GUI
│   ├── config.c      # Парсинг конфигурации
│   ├── network.c     # Сетевые соединения + SSL/TLS
│   ├── imap.c        # IMAP протокол
│   ├── smtp.c        # SMTP протокол
│   ├── ui.c          # ncurses TUI
│   └── gui.c         # GTK+3 GUI
├── include/          # Заголовочные файлы
├── config/           # Примеры конфигурации
└── docs/             # Документация
```

### Взаимодействие модулей

**TUI версия:**
```
main.c
  ├─> config.c      (загрузка настроек)
  ├─> network.c     (TCP + SSL/TLS соединения)
  ├─> imap.c        (получение писем)
  │     └─> network.c
  ├─> smtp.c        (отправка писем)
  │     └─> network.c
  └─> ui.c          (ncurses TUI интерфейс)
        ├─> imap.c
        └─> smtp.c
```

**GUI версия:**
```
main_gui.c
  ├─> config.c      (загрузка настроек)
  ├─> network.c     (TCP + SSL/TLS соединения)
  ├─> imap.c        (получение писем)
  │     └─> network.c
  ├─> smtp.c        (отправка писем)
  │     └─> network.c
  └─> gui.c         (GTK+3 GUI интерфейс)
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

## Ограничения

- Поддержка только текстовых писем (HTML не рендерится)
- Нет поддержки вложений
- Простая фильтрация (по теме/отправителю в будущих версиях)
- Один ящик за сеанс (только INBOX)

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

## Дальнейшее развитие

Планируемые функции:
- [ ] Поддержка вложений
- [ ] Фильтрация писем
- [ ] Поиск по письмам
- [ ] Работа с несколькими папками
- [ ] Адресная книга
- [ ] HTML рендеринг (упрощенный)
