

## Quick start
```bash
# configure + build (Release)
cmake --preset default -DCMAKE_BUILD_TYPE=Release
cmake --build build

# run tests
ctest --test-dir build --output-on-failure
```
