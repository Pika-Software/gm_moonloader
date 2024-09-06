# gm_moonloader
Integrate **[Moonscript][1]/[Yuescript][2]** into your Garry's Mod!

This binary module seamlessly integrates [Moonscript][1] and [Yuescript][2] into Garry's Mod, by adding include of plain `.moon`/`.yue` files and support for auto-reload.

Just add `require "moonloader"` line into your autorun!

## How to install
1. Go to [latest release](https://github.com/Pika-Software/gm_moonloader/releases/latest)
2. Download binary for your OS and Garry's Mod branch. For example `gmsv_moonloader_linux32.dll`
3. Put downloaded binary into `<Your Garry's Mod Folder>/garrysmod/lua/bin/` (if it doesn't exists then create folder)
4. Enjoy! 🎉

## How to use
1. Add require before any `AddCSLuaFile` or `include`
```lua
require "moonloader"
```
2. Include your `.moon`/`.yue` file with `include`
```lua
-- you need to pass `.lua` to include since Garry's Mod wont accept anything else
-- but gm_moonloader will find a .moon file and load it
include "example/init.lua" -- Will automatically generate .lua from .moon/.yue in garrysmod/cache/moonloader/lua
```

## Notes
* Compiled `.moon`/`.yue` files are stored in `garrysmod/cache/moonloader/lua` folder. This folder is cleaned up after each startup.

## Example
```lua
-- autorun/example_autorun.lua
if SERVER then
    require "moonloader"
end

AddCSLuaFile "example/init.lua"
include "example/init.lua"
```
```lua
-- example/init.moon
print "Hello from Moonscript! ##{i}" for i = 1, 5 
```

## API
```lua
---- Constants ----
moonloader._NAME = "gm_moonloader"
moonloader._AUTHORS = "Pika-Software"
moonloader._VERSION = "<current version, e.g. 1.2.3-main.2150cd6>"
moonloader._VERSION_MAJOR = 1
moonloader._VERSION_MINOR = 2
moonloader._VERSION_PATCH = 3
moonloader._BRANCH = "main"
moonloader._COMMIT = "2150cd6"
moonloader._URL = "https://github.com/Pika-Software/gm_moonloader"

---- Functions ----
-- Compiles given moonscript code into lua code
-- and returns lua_code with compiled line and char offset from moonCode
-- If fails, returns nil and error reason
-- Same as moonscript.to_lua (https://moonscript.org/reference/api.html)
luaCode: string/nil, lineTable: table/string
    = moonloader.ToLua(moonCode: string)

-- Compiles given yuescript code into lua code
-- See https://yuescript.org/doc/#lua-module
luaCode: string/nil, err: string/nil, globals: table/nil
    = moonloader.yue.ToLua(yueCode: string, options: table/nil)

-- Recursively compiles and caches all .moon files in given lua directory
-- Use this to add compiled .lua files into Source filesystem
-- Returns nothing
moonloader.PreCacheDir(path: string)

-- Tries to compile given file in lua directory
-- and return true if successful, otherwise false
success: bool = moonloader.PreCacheFile(path: string)
```

## Compilation
1. Clone this repo with submodules
```bash
git clone https://github.com/Pika-Software/gm_moonloader --recursive
```

2. Clone my [garrysmod_common](https://github.com/dankmolot/garrysmod_common) 
repo with branch `master-cmake` or `x86-64-cmake`
```bash
git clone https://github.com/dankmolot/garrysmod_common --branch=master-cmake --recursive
```

3. Configure cmake project
```bash
cd gm_moonloader
mkdir -p build && cd build
cmake .. -DGARRYSMOD_COMMON_PATH="../../garrysmod_common" -DBUILD_SHARED_LIBS=OFF
# Optionally also use -DAUTOINSTALL=<path to garrysmod/lua/bin>
```

4. Build it!
```
cmake --build . -j -t moonloader --config Release
```

5. (Optional) Configure project to build 32bit library
```bash
# On Windows
cmake -A Win32 .. # and other options...
# On Linux
cmake .. -DCMAKE_C_FLAGS="-m32" -DCMAKE_CXX_FLAGS="-m32" # and other options...
```

6. (Optional) Configure client-side library
```bash
cmake .. -DCLIENT_DLL=ON
```

## Contributing
Feel free to create issues or pull requests! ❤️

## Licenese
[MIT License](/LICENSE)

[1]: <https://github.com/leafo/moonscript> "Moonscript"
[2]: <https://github.com/pigpigyyy/Yuescript> "Yuescript"
