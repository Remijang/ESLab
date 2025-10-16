# GATT lib compilation guide

將 `scanner.c` 放到 `gattlib/example/ble_scan/` 目錄底下，並將該目錄中的 `CMakeLists.txt` 加上以下

```
pkg_search_module(GLIB REQUIRED glib-2.0)
include_directories(
    ${GLIB_INCLUDE_DIRS}
)
target_link_libraries(... ${GLIB_LIBRARIES} ...)
```

之後就可以在該目錄底下用

```bash
make
sudo ./ble_scan
```
來跑起程式。