# How to run Tests
This guide for running the tests

## Example on Windows MSVC
```PowerShell
cmake --preset release-vcpkg-msvc -Dtests="on" -Dsanitizers="on"
cmake --build --preset release-vcpkg-msvc
ctest --preset release-vcpkg-msvc
```
