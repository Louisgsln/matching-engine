# Matching Engine C++ + Binance API

Moteur de matching d'ordres financiers haute performance en C++17. Il est connecté à l'API Binance (REST et WebSockets) et inclut une interface CLI en temps réel (split-screen) ainsi qu'un bot de stress-test intégré mesurant le débit d'exécution (TPS).

## Architecture

```text
┌────────────────────────────────────────────────────────────────────────┐
│                               ConsoleUI                                │
│                       (Interface utilisateur CLI)                      │
└───────┬──────────────────┬──────────────────┬───────────────┬──────────┘
        │                  │                  │               │
        ▼                  ▼                  ▼               ▼
┌───────────────┐  ┌───────────────┐  ┌───────────────┐ ┌──────────────┐
│MatchingEngine │  │   OrderBook   │  │ BinanceClient │ │BinanceWebSock│
│ (Coordinator) │◄─│  (Storage +   │  │ (REST calls)  │ │(Live Stream) │
│ Stats, Valid. │  │   Matching)   │  │libcurl + JSON │ │ Boost.Beast  │
└───────┬───────┘  └───────┬───────┘  └───────────────┘ └───────┬──────┘
        │                  │                                    │
        ▼                  ▼                                    ▼
┌───────────────┐  ┌───────────────┐                    ┌──────────────┐
│  Instrument   │  │  Order/Trade  │                    │ LiveDisplay  │
│ (Référentiel) │  │  (Entities)   │◄───────────────────┤(Split-Screen)│
└───────────────┘  └───────────────┘                    └──────────────┘

## Dépendances

- **C++17** (GCC 10+ / Clang 12+ / MSVC 2019+)
- **libcurl** (requêtes HTTP vers Binance)
- **nlohmann/json** (parsing JSON)
- **CMake 3.16+**
- **Boost (Beast/Asio) (Connexions WebSockets)**
- **OpenSSL (Connexions sécurisées WSS/HTTPS)**

### Installation (Ubuntu/Debian)

```bash
vcpkg install curl nlohmann-json boost-beast boost-asio openssl
```

## Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
cmake --build . --config Release
```

## Usage

```bash
./matching_engine              # (Linux)
.\Release\matching_engine.exe  # (Windows)
```

### Commandes principales

| Commande    | Description                                     |
|-------------|------------------------------------------------ |
| `stream`    | Interface Live Split-Screen + StressTest        |
| `addinstr`  | Charger un instrument depuis Binance            |
| `bticker`   | Ticker 24h Binance                              |
| `bbook`     | Order book Binance                              |
| `bklines`   | Candlesticks Binance                            |
| `btrades`   | Trades récents Binance                          |
| `import`    | Importer le book Binance dans le moteur local   |
| `order`     | Créer un ordre BUY/SELL                         |
| `book`      | Afficher le book local                          |
| `trades`    | Historique des trades locaux                    |
| `status`    | Statistiques du moteur                          |
| `start`     | Démarrer le matching en arrière-plan            |
| `stop`      | Arrêter le matching                             |
| `symbol X`  | Changer le symbole actif                        |

### Workflow typique

```
❯ addinstr          # Charge BTCUSDT depuis Binance
❯ stream

ou

❯ addinstr          # Charge BTCUSDT depuis Binance

❯ bticker           # Voir le prix actuel
❯ import            # Importer 5 niveaux du book Binance
❯ book              # Voir le book local
❯ order             # Placer un ordre
❯ trades            # Voir les trades exécutés
❯ status            # Stats du moteur
```

## API Binance utilisée

Endpoints publics (pas d'authentification requise) :

- `GET /api/v3/ping` — Connectivité
- `GET /api/v3/time` — Heure serveur
- `GET /api/v3/depth` — Order book
- `GET /api/v3/ticker/24hr` — Ticker 24h
- `GET /api/v3/exchangeInfo` — Informations sur les symboles
- `GET /api/v3/klines` — Candlesticks OHLCV
- `GET /api/v3/trades` — Trades récents

WebSockets (Temps réel) :

- wss://stream.binance.com:9443/ws/<symbol>@depth20@100ms — Carnet d'ordres partiel
- wss://stream.binance.com:9443/ws/<symbol>@trade — Flux des trades en direct