# zlib 1.3.1

内置源码，非本项目代码，**不要改**。

| 项 | 值 |
|---|---|
| 版本 | 1.3.1（2024-01-22） |
| 来源 | https://github.com/madler/zlib/releases/tag/v1.3.1 |
| 许可 | zlib License，见 `LICENSE` |
| 引入日期 | 2026-09-05 |

## 裁剪说明

只保留了参与编译的 15 个 `.c` 和 11 个 `.h`，官方包里的 `configure` /
`CMakeLists.txt` / `test/` / `contrib/` / 各平台移植目录全部丢弃——工程由
`gun_bro_re.vcxproj` 直接列文件编译，这些用不上。

`zconf.h` 用的是官方包里已生成好的默认版本，没跑 `configure`。

## 本项目实际用到的 API

目前只有 `CBigFileReader::GetResourceByIndex` 里的一个 `uncompress()`。
其余文件（deflate / gz*）一并编进来，是为了省掉日后用到时再回来补的麻烦，
链接器会把没引用到的段丢掉。

## 升级方式

下载新版官方发布包，按上面的清单覆盖这些文件，然后核对
`gun_bro_re.vcxproj` 里的文件列表有无增减。
