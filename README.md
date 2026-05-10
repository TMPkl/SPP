# Stowarzyszenie Upitych Artystów - System Synchronizacji Rozproszonej

> **Status**: Projekt w trakcie realizacji

## About

Implementacja rozproszonego algorytmu synchronizacji dla sieci IoT opartej na zmodyfikowanym algorytmie Lamporta. System umożliwia dynamiczną formację grup procesów (kółek poetyckich), negocjację zasobów oraz koordynację działań między węzłami bez centralnego serwera.

## Problem

P niezależnych procesów musi się automatycznie organizować w grupy o rozmiarze K, uzgadniać podział zasobów (alkohol, zagrycha, rola "sępa") zapewniając równomierny rozkład obowiązków. Każdy proces może losowo zdecydować się nie uczestniczyć, a czasem odpoczynku różni się dla każdego węzła.

> dokładny "fabularny" opis tutaj [tutaj](Tematy.md)

## Architektura

### Struktura kodu

```
software_esp/SPP/           Firmware ESP32 (C)
├── src/                    Implementacja algorytmu
│   ├── lamportTS.c         Zegar logiczny Lamporta
│   ├── esp_now_receiver.c  Komunikacja bezpośrednia
│   ├── state_machine.c     Obsługa stanów
│   ├── mac_manager.c       Zarządzanie adresami MAC
│   └── mqueue.c            Kolejka wiadomości
└── include/                Headery, konfiguracja

server/Serial_redirect/     Serwer (Go)
└── main.go                 Tunel dla logów 
```

### Komunikacja

- **ESP-NOW**: Bezpośrednia komunikacja między urządzeniami (bez WiFi)
- **Kolejka rozproszona**: Utrzymywana przez każdy proces; porządek określony przez `(ts Lamporta, MAC)`
- **Wiadomości**:
  - `REQ` — żądanie dołączenia do kolejki
  - `ACK` — potwierdzenie
  - `WELCOME` — utworzenie kółka
  - `HELLO` — uzgodnienie zasobów
  - `REL` — zakończenie kółka i zwolnienie konjów

## Algorytm

### Stan maszyny

> **więcej o tym w pliku [tutaj](opis.md)

```
KACUJE → WYSYLAM_REQ → JESTEM_W_KOLEJCE → UMAWIAM_IMPREZE → IMPREZA → KACUJE
```

- **KACUJE**: Odpoczynek (czas losowy)
- **WYSYLAM_REQ**: Wysłanie żądania do wszystkich procesów, oczekiwanie na ACK
- **JESTEM_W_KOLEJCE**: Monitoring pozycji w kolejce
- **UMAWIAM_IMPREZE**: Negocjacja zasobów via `HELLO`
- **IMPREZA**: Aktywna sekcja krytyczna
- Po `REL`: Powrót do KACUJE

### Przydzielanie zasobów

Każdy proces ma **deficit** dla każdego zasobu. Algorytm:
1. Procesy wymieniają się deficytami w `HELLO`
2. Dla każdego zasobu — uczestnik z największym deficytem zostaje przydzielony
3. Tablica przydziałów jest deterministyczna na wszystkich węzłach

| K mod 3 | Sępi | Alkohol | Zagrycha |
|---------|------|---------|----------|
| 0       | K/3  | K/3     | K/3      |
| 1       | K/3+1| K/3     | K/3      |
| 2       | K/3+1| K/3+1   | K/3      |

## Technologie

- **Firmware**: ESP-IDF, FreeRTOS CMake, 
- **Platforma**: ESP32-S3
- **Komunikacja**: ESP-NOW, HTTP (logi)
- **Synchronizacja**: Algorytm Lamporta (logiczne zegary), Kolejka top x procesów oparta na ping - pong
- **Serwer**: Go (przekierowanie logów, strona, OTA (ostatecznie nieużywane ale endpoint jest gotowy))
- **Storage**: NVS (zmieoniony MAC)

## Funkcjonalności

- [x] Algorytm decentralizacyjny 
- [ ] Deterministyczne przydzielanie zasobów
- [x] Zmieniony MAC z ID w ostatnim bajcie w pamięci NVM
- [x] Przekirowanie logów przez HTTP
- [x] Obsługa utraty wiadomości (zaimplementowane na potrzeby debugowania, potem z tego zrezygnujemy)
- [ ] OTA updates (rezygnacja z implementacji)

## Logi

Logi dostępne na: `https://logs.kaleszynski.xyz/` (real-time)  **z racji na to że projekt jest nadal w budowie, urzadzenia nie sa podłączone 24/7**
Wysyłane: UART + HTTP POST na `/event`
