# CBZ RTS Game

This is the **CBZ RTS** project built on top of the CBZ framework.

## 🛠️ Building the Project

### 1. Clone the Repository
```bash
git clone --recursive git@github.com:MokaHiko/cbz_game.git
```

### 2. Download the Assets
Run this from the root of the repository:

```bash
python download_assets.py
```

### 3. Configure with CMake

```bash
cmake -S ./ -B ./build
```
You can pass additional options when configuring:
| Option             | Default | Description                                                     |
| ------------------ | ------- | --------------------------------------------------------------- |
| `CBZ_BUILD_EDITOR` | ON      | Builds the editor application                                   |
| `CBZ_BUILD_SHARED` | OFF     | Builds CBZ libraries as shared (`.dll`/`.so`) instead of static |
| `CBZ_BUILD_TESTS`  | OFF     | Builds test binaries                                            |

## License
 
The MIT License (MIT)

Copyright (c) 2025 Moka

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.