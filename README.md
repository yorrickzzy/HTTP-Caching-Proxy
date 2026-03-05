### Multithreaded HTTP Proxy
### 1. Implemented Features

Multithreaded HTTP proxy (one thread per connection)

GET with RFC-compliant caching

Expiration handling (Cache-Control, Expires)

Conditional revalidation (ETag, Last-Modified)

Proper 304 handling

POST forwarding (Content-Length & chunked)

CONNECT tunneling (HTTPS support)

400 Bad Request handling

502 Bad Gateway handling (origin unreachable / corrupted response)

Thread-safe logging

Docker deployment

Automated regression test suite

### 2. Architecture
```
docker-deploy/
  src/
    http/
      http_types.hpp
    cache/
      cache.hpp
      cache.cpp
      get_pipeline.hpp
      get_pipeline.cpp
    log/
      logger.hpp
      logger.cpp
    proxy/
      server.cpp
```

### 3. Cache Policy

Only cache GET 200 responses

Respect:

- Cache-Control

- Expires

- no-store

- no-cache

- must-revalidate

Conditional headers used when applicable:

- If-None-Match

- If-Modified-Since

Replacement policy: FIFO

Default cache size: 1000 entries

### 4. Error Handling

Implemented:

- 400 Bad Request

- 502 Bad Gateway

- Graceful failure (no crash on malformed input)

### 5. Logging

All logs are written to:
```
/var/log/erss/proxy.log
```
Each request logs:

- Request received

- Cache state

- Origin request

- Origin response

- Cache decision

- Final response

- Error conditions

### 6. Docker Deployment

Build
```
cd docker-deploy
sudo docker-compose build
```
Run
```
sudo docker-compose up
```
Proxy listens on:
```
localhost:12345
```
Logs are mounted to:
```
docker-deploy/logs/proxy.log
```

### 7. Automated Test Suite

Run:
```
cd docker-deploy
./tests/run_tests.sh
```
Test coverage includes:

- Basic GET

- Cache behavior

- 304 revalidation

- Malformed CONNECT → 400

- Unsupported method → 400

- Concurrency (parallel GETs)

- POST forwarding

- Corrupted origin response → 502

Contributor: Zeyuan Zhang / Jiayi Li
