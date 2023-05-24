# gm_moonloader
Integrate moonscript into your Garry's Mod!

This binary module seamlessly integrates [moonscript](https://moonscript.org/) into Garry's Mod, by adding include of plain `.moon` files and support for auto-reload.

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
2. Include your `.moon` file with `include`
```lua
-- you can use example/init.moon, but for sake of compability
-- I suggest to use .lua instead of .moon
include "example/init.lua" -- Will automatically generate .lua from .moon in garrysmod/cache/moonloader/lua
```
3. (optional) before using `AddCSLuaFile` or finding lua files with `file.Find` I suggest to use `moonloader.PreCacheDir("yourdirectory")` to cache .lua files from all .moon files

## Notes
* Compiled `.moon` files are stored in `garrysmod/cache/moonloader/lua` folder. This folder is cleaned up after each startup.
* To properly use `AddCSLuaFile`, precache folder with `moonloader.PreCacheFolder` before adding .lua file
* `AddCSLuaFile` or `CompileFile` only supports `.lua` extensions, use precaching.

## API
```lua
---- Functions ----
-- Compiles given moonscript code into lua code
-- and returns lua_code with compiled line and char offset from moonCode
-- If fails, returns nil and error reason
-- Same as moonscript.to_lua (https://moonscript.org/reference/api.html)
lua_code: string/nil, line_tabel: table/string
= moonloader.ToLua(moonCode: string)

-- Recursively compiles and caches all .moon files in given lua directory
-- Use this to add compiled .lua files into Source filesystem
-- Returns nothing
moonloader.PreCacheDir(path: string)

-- Tries to compile given file in lua directory
-- and return true if successful, otherwise false
success: bool = moonloader.PreCacheFile(path: string)

---- Hooks ----
-- Executes when .moon file was compiled
-- Path is relative to lua directory
GM:MoonFileCompiled(path: string)
```

## Contributing
Feel free to create issues or pull requests! ❤️

## Licenese
[MIT License](/LICENSE)
