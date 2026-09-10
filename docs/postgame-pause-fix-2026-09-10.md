# 暂停音乐与结算图标、页签

后续用户实测发现短时检查漏掉Perfect Waves停止循环，以及普通僵尸模型缩放异常；已补30秒检查及修复，见[循环与缩放记录](postgame-loop-enemy-scale-2026-09-10.md)。下文保留首轮3秒验证的历史边界。

## 方案与验收

用户授权：暂停继续战斗音乐但降低音量；结算XPlo、XP、Perfect Waves及Horde替代图标播放原闪烁动画；Overview/Casualties直接切换，无横扫。

- [x] 真实暂停/道具菜单路径验证音乐继续播放、音量减半、恢复及关闭音乐设置。
- [x] 恢复原粒子绑定及BIG Sprite动画时钟，以实际像素变化验证四个图标。
- [x] 真实页签点击不触发通用横扫，其他页面导航继续横扫。
- [x] Release构建、专项红绿与相关回归，更新交接。

依据：`CGunBros::OnSuspend` :78232→`CBGM::SetVolume(0.5)`，`OnResume` :78096→1.0；`CBGM::SetVolume` :59971再乘0.3。`CMenuPostGame::UpdateCurrentView` :165242仅更新当前视图的控件。BT：`sprite_archetype.bt`、`sprite_global.bt`、`ui_movie.bt`、`entries/particle_effect.bt`。

## 原因与实现

1. 暂停路径调用了停止SDL流的 `SetPaused`。改为原版音量倍率，暂停/道具选择器为0.5，游戏、结算和菜单恢复1.0。`SetEnabled`仍统一控制音乐开关，关闭时改变倍率不会打开声音。
2. 结算图标一直绘制第0帧，同时遗漏专用控件的粒子层。原 `MENU_POST_GAME_WRAPUP` 位于ARMv7 VA `0x403350`，组配置第二字段（表+40）为11；`CMenuOptionGroup::CreateMenuOption` :175443中case11创建 `CMenuPostGameOption`。其 `Bind` :249937分别绑定Sprite及Particle，`Update` :249880推进两者，`Draw` :249755在原Movie区域1中心先画粒子再画图标。初查沿普通 `CMenuIconOption` 的线索不足以解释静态星星，现已由专用控件消费链纠正。
3. `CMenuDataProvider::CreateContentParticle` :149304从MDS记录+48读取有符号short，并在原包的type11查找。扩展 `src/tools/extract_menu_statics.py` 生成 `OriginalPostGameParticleData.inc`，保留原程序SHA256和来源；只提取原生绑定，效果模板、发射间隔、轨迹、缩放、透明度和图像均在运行时读取BIG。
4. `GameMenu`为每个结算条目持有独立粒子播放器，进入新结算时重置，当前Overview更新，Casualties期间保持其状态。复用现有 `WeaponEffects` 粒子执行器。所有图标推进原Sprite帧时钟。
5. Overview/Casualties属于同一 `CMenuPostGame` 的current view。将内部page28与27归入同一菜单分支，直接换内容，保留跨分支页面的原横扫。

| MDS_ICON_POSTGAME条目 | 原Sprite引用 | 原Sprite周期 | 原Particle引用 |
|---|---|---|---|
| 0 XPlo | core 0:85 | 900ms | core type11:5 |
| 1 XP | core 0:86 | 600ms | core type11:6 |
| 4 Perfect Waves | core 33:3 | 单帧100ms | core type11:4 |
| 5 Best Streak（Horde） | core 33:4 | 单帧100ms | core type11:14 |

容器为原 `GLU_MOVIE_WRAPUP_BOX`，core Movie20、handle `0x030004BB`，解包样本 `big_360_out/pack0_core_xga/0xf4e02223/pack0_core_xga_0103_0x2a3d62.bin`。容器自身只有time0关键帧，火花来自上述独立粒子。

## 验证记录

永久研究菜单新增77 / `--postgame-presentation-check`，加入Core回归；保留1–76。

- 修复前：`out/postgame-presentation-before.log`退出1，四图标像素无变化，双向页签各触发一次横扫，共6失败；`out/pause-bgm-before.log`退出1，3次暂停/道具菜单均停止流并保持0.30。
- 修复后：四图标在3000ms采样中分别有150、150、149、150个变化帧，最大粒子数6、9、29、39；双向页签横扫次数均0。截图 `out/postgame-icon-{0,1,4,5}-1000.png`和`out/postgame-tab-{0,1}.png`，已查看实际渲染的星星火花与整页布局。
- 真实SDL零设备增益验证沿原HUD点击和Esc/空格路径，暂停流标志始终0，暂停0.15、恢复0.30。实际声音设备用于检查传输状态，保持静音外放。
- 初次将控制回归驾驶器接到原存档音频样本时，因该样本未配置控制测试要求的双枪/道具而失败；改为在已有 `--profile-play-check` 的隔离装备样本中注入真实SDL播放器。未放宽控制断言。
- 注入跨场景音乐后，测试入口原有的逐场窗口会在结束时 `SDL_Quit`，使下一场持有失效音频流，出现访问异常。测试改为与正式前端相同的共享窗口生命周期，音乐先于窗口析构；续玩回归恢复退出0。

最终运行 `bin/x64/Release/gun_bros_research.exe --mute`，追加下列参数，全部退出0：

| 参数 | 覆盖 | 日志 |
|---|---|---|
| `--postgame-presentation-check` | 四个原粒子、实际像素变化、双向无横扫 | `out/postgame-final-postgame-presentation-check.log` |
| `--profile-play-check` | 13步原HUD/快捷键、SDL连续播放与音量、保存后续玩至第4波 | `out/postgame-final-profile-play-check.log` |
| `--audio-transitions-check` | 两兄弟四次切枪音效、加载/结算/精炼音乐、暂停音量下开关音乐 | `out/postgame-final-audio-transitions-check.log` |
| `--postgame-menu-check` | 原结算分数、页签、Horde、退出、不重复发奖 | `out/postgame-final-postgame-menu-check.log` |
| `--loading-wipe-check` | 原CG/STR、跨分支横扫和过渡中输入保护 | `out/postgame-final-loading-wipe-check.log` |

MSBuild `gun_bro_re.slnx /p:Configuration=Release /p:Platform=x64 /v:minimal /nologo`退出0；正式游戏与研究EXE均更新，日志 `out/postgame-pause-build-final.log`。

本次未做耳听、原iOS随机粒子的逐帧同种子对比；源码/资源、实际渲染和SDL传输验证不代表逐像素或逐采样完全一致。原BIG、解包样本、iOS程序与源存档只读。
