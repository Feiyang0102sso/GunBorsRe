# 音频归属与原版对齐

日期：2026-09-19。用户要求战斗音频回归 gameplay/audio，并追问为何没有复用原版逻辑。

## 本次方案与任务

1. 核对当前消费者与原版声音调用链，区分业务调度和平台输出。
2. 将 `ZCombatAudio.h/.cpp` 从 host 移入 `gameplay/audio/`，同步引用和当前映射，不删除旧注释。
3. 保留共用 `engine/platform/ZAudioPlayer.*`：战斗、CBGM、菜单试听、启动流程、Viewer 均在使用；不让 engine 反向依赖 gameplay。
4. 构建并运行相关音频回归。本次移动不冒充原版声音系统复刻完成，也不在目录调整中静默改变已有播放行为。

## 原版证据与当前缺口

依据 `_prep/_IDA_OUT/gunbros_3.6.0_IOS.c`：

- `CSoundQueue::PlaySound`（102896）查找已加载的声音条目并调用媒体播放器。
- `CMediaPlayer::PlayInternal`（363259）为播放请求创建独立 `CSoundEvent`，记录媒体、循环等状态，创建成功后加入事件链表。
- `CSoundEvent_Cocoa::SetGain`（303186）将播放器音量和事件音量相乘并缩放，再调用 OpenAL 设置声源增益。旧播放器注释把此处记为 SetVolume，函数名应以本次核对的 SetGain 为准。

当前战斗脚本事件和 BIG 声音引用有沿用原资源，但播放事件管理没有完整复刻。`ZCombatAudio` 仍有同帧同 WAV 合并、普通音效停止旧声音再播放、动作音等待前一段结束等自建策略；源码保留了当时为处理密集重复音效而添加这些策略的历史说明。`entries/sound_effect.bt` 也记录过用户要求合并同帧同 WAV；同时明确资源加载登记的查重不是播放帧去重。这些策略不是 Windows 必需的兼容操作，不能归为已对齐的原版行为。

应恢复的边界是：游戏声音请求、独立事件、停止／循环／暂停生命周期与音量合成按原版消费函数实现；底层 Cocoa/OpenAL 输出由 SDL 后端承接。平台 API 需要替换，不意味着上层事件语义也必须自建。完整恢复需同时验证播放句柄、重叠声部、音量来源和循环释放，不能只删去去重代码或把当前类改名为 CSoundQueue 就宣称完成。

本次保留 `ZCombatAudio` 名称，明确它仍是待对齐的实现。没有将自建规则仅凭改名包装为原类。

## 验证

Debug Game、Viewer、Tests 构建成功，退出 0。`pwsh -File tests/run.ps1 -Configuration Debug -Phase Core -Case audio-transitions,weapon-effects -NoBuild` 两项通过，受保护资源／存档／配置变化 0；音频检查使用静音参数及零增益真实 SDL 流，记录声音、设备、流数量均为 1，暂停和设置保留断言通过。`git diff --check` 通过。

证据归档：`obj/audio-ownership-20260919/`。本次未重建 Release，未新增重复实现的测试；验证的是迁移没有破坏现有行为，不是原版声音语义已恢复。
