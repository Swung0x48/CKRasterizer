# CKRasterizer

3rd party rasterizers (aka. RenderEngines) for Virtools.

## Build instructions

1. Get glew binaries and extract the `lib` directory into `vendor/glew`.

2. Generate build files

```
cmake .. -DVIRTOOLS_SDK_PATH=<path to virtools sdk> -DCMAKE_BUILD_TYPE=Release -A Win32
```

3. Build

```
cmake --build . --config Release
```

or

```
msbuild CKRasterizer.sln
```
