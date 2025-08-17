# rd2aria

A simple tool with a CLI to simplify the process of downloading torrents through Real-Debrid using aria2.

## Features

- Add magnet links to Real-Debrid
- Unrestrict links
- Download files using `aria2c`
- Simple CLI

## Build

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/rda
```

## API Token

- Linux/macOS:

```bash
export REAL_DEBRID_API_TOKEN=your_token_here
./myapp
```

- Windows (PowerShell):

```powershell
$env:REAL_DEBRID_API_TOKEN=your_token_here
.\myapp.exe
```
