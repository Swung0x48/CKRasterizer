# CKRasterizer

3rd party rasterizers (aka. RenderEngines) for Virtools.

## Build instructions

1. Generate build files

```
cmake .. -DVIRTOOLS_SDK_PATH=<path to virtools sdk> -DCMAKE_BUILD_TYPE=Release -A Win32
```

2. Build

```
cmake --build . --config Release
```

or

```
msbuild CKRasterizer.sln
```