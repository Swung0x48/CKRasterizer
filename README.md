# CKRasterizer

Custom rasterizers for Virtools.

## Subprojects & Current Status

| Project  | Description | Status |
| -------- | ----------- | ------ |
| CKDX9Rasterizer | DirectX 8 Rasterizer bundled with Virtools 2.1.0.14 reverse engineered and ported to DirectX 9 with Direct3D 9Ex support. | Mostly complete |
| CKGLRasterizer | OpenGL Rasterizer targeting OpenGL 4.3+ built from ground up (*) | Feature complete for running _Ballance_, ~90% feature complete for general use. Not yet optimized. |
| CKDX11Rasterizer | DirectX 11 Rasterizer with DXGI 1.5+ AllowTearing feature adaptively enabled to eliminate present latency | Feature mostly complete for running _Ballance_. Minor details differs from fixed pipeline DirectX 8/9 rasterizer. Not yet optimized (by any means!). |
| CkVkRasterizer | (Work in progress) | Context creation |
| CkVkRasterizer | (Also work in progress) | Device enumeration |

All of these rasterizers are built with the game _Ballance_ in mind. You may be able to build them against a newer Virtools SDK with a few changes, but do not expect it to work correctly all the time. Don't even count it on working with other games built with Virtools 2.1.

(*) : Bits of logic from the DX9 Rasterizer are used to retain compatibility with the rasterizer interface.

## Build instructions

0. Clone this repository with all its submodules (`git clone --recurse-submodules ...`).

1. Get glew binaries and extract the `lib` directory into `vendor/glew`.

2. Generate build files

```
cmake .. -DVIRTOOLS_SDK_PATH=<path to virtools sdk> -DCMAKE_BUILD_TYPE=Release -DTRACY_ENABLE=OFF -A Win32
```

3. Build

```
cmake --build . --config Release
```

or

```
msbuild CKRasterizer.sln
```

## License

BSD-3-Clause
