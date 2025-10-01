# real-debrid-download-helper

NOTE: You must have a premium Real-Debrid subscription in order to use the API.
A command line tool to simplify the process of downloading torrents through Real-Debrid.

## Features

- Simple CLI
- Add magnet links to Real-Debrid
- Wait until torrent is cached and obtain unrestricted links
- Optionally display the unrestricted links
- Optionally dump the links into a .txt file in folder of choice
- Optionally download files using `aria2c`

## Known BUGS

- The token bucket implementation to control rate of API calls is currently broken.
  - WORKAROUND: The initial tokens has been set to 50, so if the number of files in any torrent is less than 50, everything should work fine.

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
