# rd2aria

A simple tool with a TUI to simplify the process of downloading torrents through Real-Debrid using aria2.

## Features

- Add magnet links to Real-Debrid
- Monitor progress and unrestrict direct links
- Download files using `aria2c`
- Simple TUI (planned)

## Build

```bash
cmake -B build -S .
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
