# real-debrid-download-helper

A command line tool to simplify the process of downloading torrents through Real-Debrid.

NOTE: You must have a premium Real-Debrid subscription in order to use the API. Additionally, in order to download through aria2, please [install](https://github.com/aria2/aria2) it separately.

## Features

- Simple CLI
- Add magnet links to Real-Debrid
- Wait until torrent is cached and obtain unrestricted links
- Optionally display the unrestricted links
- Optionally dump the links into a .txt file in folder of choice
- Optionally download files using `aria2c`

## Build

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/rddl
```

## Options and Flags

    -h,     --help                      Print this help message and exit
    -t,     --token     TEXT            Set API token and save it locally
    -m,     --magnet    TEXT REQUIRED   Magnet link
    -l,     --links                     Print unrestricted links
    -o,     --output    TEXT            Specify path for output .txt file
    -a,     --aria2                     Start download using aria2

## API Token

Either pass the token using the option -t/--token, or add the following to your environment variables:

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
