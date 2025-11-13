# Архитектура cterm

## Обзор

cterm - модульный почтовый клиент, написанный на C с использованием минимальных зависимостей.

## Модульная структура

### 1. config.c/h - Конфигурация

**Назначение:** Парсинг конфигурационного файла ~/.cterm.conf

**Основные функции:**
- `config_load()` - загрузка конфигурации из файла
- `config_free()` - освобождение ресурсов
- `config_print()` - отладочный вывод конфигурации

**Формат конфига:**
```
key = value
```

**Структура данных:**
```c
typedef struct {
    char imap_server[256];
    int imap_port;
    int imap_use_ssl;
    char imap_username[256];
    char imap_password[256];
    char smtp_server[256];
    int smtp_port;
    int smtp_use_ssl;
    int smtp_use_starttls;
    char smtp_username[256];
    char smtp_password[256];
    char email_address[256];
    char display_name[256];
} Config;
```

### 2. network.c/h - Сетевые соединения

**Назначение:** Низкоуровневая работа с TCP и SSL/TLS

**Основные функции:**
- `net_init_ssl()` - инициализация OpenSSL
- `net_connect()` - установка TCP соединения с опциональным SSL
- `net_send()` - отправка данных
- `net_recv()` - прием данных
- `net_recv_line()` - прием одной строки
- `net_disconnect()` - закрытие соединения
- `net_cleanup_ssl()` - очистка OpenSSL

**Структура соединения:**
```c
typedef struct {
    int sockfd;           // Socket descriptor
    SSL *ssl;             // SSL connection
    SSL_CTX *ssl_ctx;     // SSL context
    int use_ssl;          // SSL enabled flag
} Connection;
```

**Особенности:**
- Автоматическое определение необходимости SSL
- Поддержка как прямого SSL, так и STARTTLS
- Обработка ошибок SSL

### 3. imap.c/h - IMAP протокол

**Назначение:** Реализация IMAP клиента для получения писем

**Основные функции:**
- `imap_connect()` - подключение к IMAP серверу
- `imap_login()` - аутентификация (LOGIN)
- `imap_select_mailbox()` - выбор почтового ящика
- `imap_fetch_emails()` - получение списка писем
- `imap_fetch_email_body()` - получение тела письма
- `imap_mark_seen()` / `imap_mark_unseen()` - управление флагами
- `imap_delete_email()` - удаление письма
- `imap_expunge()` - окончательное удаление
- `imap_disconnect()` - отключение

**Структуры данных:**
```c
typedef struct {
    unsigned int uid;
    char subject[256];
    char from[128];
    char date[64];
    int seen;
    int deleted;
    char body[4096];
} Email;

typedef struct {
    Connection conn;
    int logged_in;
    int tag_counter;      // Для уникальных IMAP тегов
    Email *emails;
    int email_count;
    int email_capacity;
} ImapSession;
```

**IMAP команды:**
- `A001 LOGIN username password`
- `A002 SELECT INBOX`
- `A003 FETCH 1:* (UID FLAGS BODY.PEEK[HEADER.FIELDS ...])`
- `A004 UID STORE <uid> +FLAGS (\Seen)`
- `A005 UID STORE <uid> +FLAGS (\Deleted)`
- `A006 EXPUNGE`
- `A007 LOGOUT`

### 4. smtp.c/h - SMTP протокол

**Назначение:** Реализация SMTP клиента для отправки писем

**Основные функции:**
- `smtp_connect()` - подключение к SMTP серверу
- `smtp_starttls()` - обновление соединения до TLS
- `smtp_auth_login()` - аутентификация (AUTH LOGIN с base64)
- `smtp_send_email()` - отправка письма
- `smtp_disconnect()` - отключение

**Структура данных:**
```c
typedef struct {
    Connection conn;
    int connected;
} SmtpSession;
```

**SMTP команды:**
- `EHLO localhost`
- `STARTTLS` (если нужно)
- `AUTH LOGIN`
- `MAIL FROM:<from@example.com>`
- `RCPT TO:<to@example.com>`
- `DATA`
- (headers and body)
- `.`
- `QUIT`

**Особенности:**
- Base64 кодирование для AUTH LOGIN
- Поддержка STARTTLS для TLS upgrade
- Автоматическое формирование заголовков (Date, From, To, Subject)

### 5. ui.c/h - Пользовательский интерфейс

**Назначение:** Ncurses TUI для взаимодействия с пользователем

**Основные функции:**
- `ui_init()` - инициализация ncurses
- `ui_run()` - главный цикл событий
- `ui_draw_email_list()` - отрисовка списка писем
- `ui_draw_email_content()` - отрисовка содержимого письма
- `ui_draw_compose()` - форма создания письма
- `ui_draw_status()` - строка состояния
- `ui_handle_input()` - обработка клавиатурного ввода
- `ui_cleanup()` - очистка ncurses

**Структура контекста:**
```c
typedef enum {
    VIEW_EMAIL_LIST,
    VIEW_EMAIL_CONTENT,
    VIEW_COMPOSE
} ViewMode;

typedef struct {
    WINDOW *main_win;
    WINDOW *status_win;
    ViewMode current_view;
    int selected_index;
    int scroll_offset;
    ImapSession *imap_session;
    SmtpSession *smtp_session;
    Config *config;
    int running;
} UIContext;
```

**Представления:**
1. **EMAIL_LIST** - список писем с прокруткой
2. **EMAIL_CONTENT** - просмотр письма
3. **COMPOSE** - создание нового письма

**Управление:**
- Навигация (стрелки, j/k)
- Выбор (Enter)
- Команды (C, D, R, Q)
- Escape для возврата

### 6. main.c - Главный модуль

**Назначение:** Точка входа и координация всех модулей

**Последовательность запуска:**
1. Парсинг аргументов командной строки
2. Загрузка конфигурации (config.c)
3. Инициализация SSL (network.c)
4. Подключение к IMAP и авторизация (imap.c)
5. Выбор INBOX и загрузка писем
6. Подключение к SMTP и авторизация (smtp.c)
7. Запуск TUI (ui.c)
8. Главный цикл событий
9. Очистка ресурсов

## Поток данных

```
┌─────────────┐
│   main.c    │
└──────┬──────┘
       │
       ├──────> config.c ────> ~/.cterm.conf
       │
       ├──────> network.c ───> TCP/SSL
       │              │
       ├──────> imap.c ────────┘
       │              │
       │              └──────> Email[]
       │                          │
       ├──────> smtp.c ───────────┘
       │              │
       │              └──────> Send
       │
       └──────> ui.c
                   │
                   ├──> ncurses (display)
                   └──> keyboard (input)
```

## Управление памятью

- **Config:** Статическая структура, очищается через `config_free()`
- **IMAP Emails:** Динамический массив, управляется через `malloc/realloc/free`
- **SSL Context:** Создается при подключении, освобождается при отключении
- **Ncurses Windows:** Создаются при инициализации UI, удаляются при выходе

## Обработка ошибок

Все функции возвращают статус:
- `0` - успех
- `-1` - ошибка

Ошибки логируются через `fprintf(stderr, ...)`.

## Многопоточность

Программа однопоточная (single-threaded). Все операции выполняются последовательно в главном потоке.

## Безопасность

1. **Пароли:** Хранятся в plain text в конфиге (chmod 600 рекомендуется)
2. **SSL/TLS:** Все соединения с серверами шифруются
3. **Валидация:** Минимальная валидация входных данных
4. **Buffer overflow:** Проверки размеров буферов с `strncpy`, `snprintf`

## Расширяемость

Для добавления новых функций:

1. **Новый протокол:** Создать модуль по аналогии с imap.c/smtp.c
2. **Новое представление UI:** Добавить ViewMode в ui.h
3. **Новые настройки:** Расширить структуру Config
4. **Новые горячие клавиши:** Обновить ui_handle_input()

## Производительность

- Оптимизирован для low-end систем (ARM, embedded)
- Минимальное использование памяти
- Без графических библиотек (только ncurses)
- Компиляция с `-O2` для release

## Зависимости

```
cterm
  ├── libc (POSIX)
  ├── ncurses (UI)
  └── OpenSSL (SSL/TLS)
```

Все библиотеки доступны на большинстве Linux систем.
