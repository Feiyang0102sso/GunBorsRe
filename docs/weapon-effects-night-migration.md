# 武器特效夜间迁移记录

开始：2026-09-17 06:32 UTC（当地 01:32）。计划约六小时，最后约 45 分钟用于最终构建、回归与交接。用户已授权方案实施，不提交 Git。

交接：2026-09-17 08:38 UTC（当地 03:38），约 2 小时 6 分钟。主组合层迁移、R03 和速度单位修正已经完成，最终验证通过；按照用户允许完成后提前收尾的要求，不为凑满六小时扩大范围。原版虚调用等未确认差异已单独登记。

**当前可运行，组合层迁移没有遗留调用入口。** Debug／Release 的 Game、Viewer、Tests 均构建成功；正式菜单、Viewer 武器播放、战斗／死亡／重生及相关 UI 均有实际运行记录。新增文件尚未提交 Git，原资源保持只读。文件归位清单见 [源码映射](source-name-map.md#夜间特效组合层迁移)，最终验证见本文末节。

后续目录整理：用户批准将共享特效的 28 个源码文件统一移入 `src/gun_bros_re/effects/`，供 UI 与战斗直接引用。本次夜间记录中的既有行为与所有权结论不变，最新路径及目录整理验证见 [归并记录](effects-directory-migration.md)。

## 起点

- 实际目录 `E:\coding_projects\c_projects\gun_bro_re`，启动时两次 `git status --porcelain=v1` 均为空。既有 Powerup、Brother、Level 归位成果保留；保留 `CBrotherPowerups.cpp`。
- 已阅读 AGENTS、五份重点文档和构建／测试脚本。工程通配收集源码，一个工程仍产出 Game、Viewer、Tests。

## 阶段一：UI 粒子独立

依据：粒子与 Sprite BT；原 `CParticleEffectPlayer::Draw` 131724、`CParticle::Draw` 133466、`CMenuPostGameOption::Draw` 249755、`CTransferEffect::Draw/Update` 174292/174361、`CPowerup::Draw/Update/Bind` 188627/188652/188745。资源继续来自 BIG，保留原静态菜单引用及来源记录。

方案与任务：

1. 提取共享 Sprite 展开／批绘制资源适配，粒子播放器负责遍历自己的粒子并提交绘制；武器复用同一实现。
2. 模式按钮、结算卡片、炼油厂直接持有播放器，共享菜单粒子池；保留位置、绘制顺序和停止语义。
3. Powerup 使用五个播放器及原 100 槽池。将其备用声音通路与粒子解耦，声音适配原样迁移。
4. 构建三产物，运行 postgame-presentation、refinery-menu、postgame-menu、powerup-play 及粒子边界检查。

验收：纯 UI 粒子不构造武器组合对象，不复制发射算法；立即停止释放粒子，停止发射保留残余粒子，重新使用与取消不泄漏池槽；无新的资源表或 ID 分支。

### 阶段一结果

- 已归位菜单与 Powerup 播放器，增加 `ZSpriteRenderer` 共用 Sprite 缓存／批绘制适配、`ZParticleResources` BIG 模板缓存、`ZCombatAudio` 声音适配。原播放器的发射算法未复制。
- Debug Game／Viewer／Tests 构建通过（退出码 0）。`obj/night-stage1-build.log` 保留构建结果；测试修复后的 Tests 构建亦为 0。
- `postgame-presentation,refinery-menu,postgame-menu,powerup-play,weapon-effects,audio-transitions` 6/6 通过，脚本退出码 0，受保护资源变化 0。日志 `obj/night-stage1-checks-fixed.log`，逐项结果及截图保留在 `obj/night-stage1-evidence/`。
- 首次音频检查超时；原生调试确认 `CRefinementManager::IsGated` 空模板访问冲突。现有音频测试未按正式前端先 Reset 绑定 BIG 模板；补齐测试准备步骤后完整检查 15.1 秒通过。未改动声音行为；异常证据 `obj/night-audio-exception-stack.log`。

## 阶段二：地图实例、锚点与回收

依据：`CParticleSystem` 133841–133966，`CParticleEffectPlayer::SetAnchor/Init/Update` 131280/131356/131499，`CBrother` native 11 138969 与锚点 134152，`CPickup::Spawn` 99893，`CEnemy::StartLinkedEffect/SpawnParticleEffectNode` 70563/70623，`CProp` native 15/16 124665/124678。弹体普通爆发另走 `CEffectLayer::AddParticleEffect`（60997），不能被地图系统容量限制。

方案与任务：

1. 恢复固定 20 槽 CParticleSystem 和独立 200 槽池，使用可失效的实例句柄区分槽的每次使用；满槽返回空，不排队或抢占。
2. 普通角色、敌人、场景物及拾取物使用地图系统。弹体效果层、强化自有池及预放置地图效果保持独立。
3. 播放器接受锚点更新；兄弟普通效果按原版跟随角色位置且角度为零。死亡位置仍可查询，角色被替换／重生前脱离旧锚点；死亡效果归属独立保留用于结算收尾。
4. 槽复用、粒子池耗尽后不补发、Stop/StopSpawning、锚点失效和归属测试；运行 pickups、prop-combat、enemies、player-death、deathmatch-feedback。

验收：20 个效果槽与 199 个可领取粒子分别耗尽；旧句柄不能停止复用后的新效果；死亡与重生不串位置；地图与强化／UI／效果层池不互相挤占。

### 阶段二结果

- Debug 三产物构建退出码 0，日志 `obj/night-stage2-build.log`。
- `weapon-effects,pickups,pickup-render,prop-combat,enemies,player-death,deathmatch-feedback` 7/7 通过，退出码 0，受保护资源变化 0。日志 `obj/night-stage2-checks.log`，结果与截图 `obj/night-stage2-evidence/`。
- 边界覆盖独立的 20 效果槽／199 粒子容量、停止后排空、立即释放、槽复用旧句柄失效、锚点变化与失效。死亡期间继续查询角色位置，DM 重生前脱离旧锚点。

## 阶段三：弹体、附属效果及实例归位

依据：BULLET BT 与原 `CBullet::Fire/Configure/Update/Draw/Bind` 62212/62251/62375–63590/62998/63599；`CBullet::SpawnEffect` 60941 区分光束附属效果和普通 CEffectLayer 爆发；构造 63810 建立容量 4 的 EffectContainer。`EffectContainer` 294598–294761、`ParticleEffectHolder` 2948xx、`TrailEffectHolder::Update` 294891、`CRibbonTrailEffect::Draw` 243694。真实 BIG 弹体继续通过现有武器及特效研究检查读取。

方案与任务：

1. 将 ZShot 状态合并进 CBullet，移动、碰撞、光束与绘制由弹体执行；资源缓存只负责 BIG／GPU 资源。
2. 恢复附属效果容器及粒子、拖尾持有者，保留停止发射后的排空与严格采样边界。
3. CLevel 持有、生成与调度弹体；地图系统、效果层和强化持有关系分别处理。Game、Viewer、研究测试共用行为。
4. 替换全部组合入口调用，独立声音适配维持原去重与声音时钟行为，删除组合层源码。

验收：同帧生成更新、枪口跟随、伤害／反弹／计数及退场尾迹不退化；容量不被隐式扩容；通过武器、特效、地雷、音频及关联角色检查。

### 阶段三、四结果

- ZShot 状态并入 CBullet；移动／碰撞与绘制分别在 `CBulletProjectile.cpp`、`CBulletDrawing.cpp`，附属效果在 `CBulletEffects.cpp`。BIG 模板／模型缓存独立为 ZBulletResources。
- 恢复 EffectHolder、EffectContainer（每弹体四槽）、ParticleEffectHolder、TrailEffectHolder、CRibbonTrailEffect、CEffectLayer。停止发射后仍由原弹体保留附属效果，直至实际完成；普通爆发属于独立效果层。光束爆发按原版使用命中端锚点。
- 已删除 `ZWeaponEffects.h/.cpp`，CLevel 直接持有弹体并调用上述原类。无同职责替代包装或转发壳；声音仍由先前原样提取的 ZCombatAudio 负责 Windows 合音／去重。
- 三产物 Debug 构建退出码 0，`obj/night-stage4-build.log`；`weapons,weapon-effects,mines,audio-transitions,powerup-play,player-death,deathmatch-feedback,enemies,prop-combat,pickup-render` 10/10 通过，退出码 0。证据 `obj/night-stage4-evidence/`。
- 随后将强化粒子持有关系收回 CBrother（六播放器、共享 25 槽池、稳定状态跨装备切换），删除 CLevel 的粒子影子实例列表。地图锚点与归属直接在 CParticleSystem，销毁 CLevel 前清除引用它的地图锚点。
- Tests 构建退出码 0，`obj/night-stage4b-build.log`；`powerup-play,deathmatch-feedback,player-death,weapon-effects,enemies,pickup-render` 6/6 通过，退出码 0。证据 `obj/night-stage4b-evidence/`。
- 中间弹体行为归位和四槽容器分别经过四项回归，`obj/night-stage3a-checks.log`、`obj/night-stage3b-checks.log` 均退出码 0；后者边界检查含四槽满、停止排空、立即停止、槽复用与严格拖尾采样间隔。

## 阶段五：R03 光束动画去启发式

证据：`bullet_template.bt`、CGameSpriteGluRef 磁盘读取；原 Bind 63647–63667 将资源动画直接绑定主体，并固定以资源动画 +1/+2 绑定两端，Draw 62998–63049 直接消费。pack5 的 BULLET 段首物理文件为 4，因此逻辑序号 104 对应物理文件 **108**，不是文件名 0104。

实际样本：`pack5_xga_0108_0x6743.bin`，235 字节，归档偏移 0x6743、逻辑 handle 0x016B。前七字节 `85 75 26 00 8B 00 01`：Sprite 包 0x00267585、原型 139、action 0、animation 1。对应 Flow 清单为同名 0108；出口 1 调用函数 0 设置电弧等参数，状态没有 Sprite 序列，不存在把主体动画修正为 0 的指令。

方案：删除 bounds 判定及减一行为，主体遵循 Flow 当前动画，两端遵循 Bind 的原资源 +1/+2。保留历史注释并注明纠正，旧“亮度谷值必须大于 25%”视觉假设改为诊断，测试检查原引用和真正绘制输出。验收：实际 BIG 样本主体 1、两端 2/3，几何、电弧、射线碰撞和释放回归通过，不按 ID 修图。

## 后续阶段

完成 R03 后核对速度单位链，再进行最终构建、相关回归及文档交接；未确认的 Viewer 手写转场表继续单独列出，不扩展为未经验证的大范围重构。

### 阶段五结果及附属效果顺序审计

- Tests 构建退出码 0（`obj/night-stage5-build.log`）；`weapons,weapon-effects` 2/2 通过，退出码 0，受保护资源变化 0。日志 `obj/night-stage5-checks.log`，证据 `obj/night-stage5-evidence/`。
- 后续审计原 SetRibbonTrail 60518、EffectContainer::Attach 294761 和 ActivateRemovalPending 62366，确认 Ribbon 与粒子须按 native 调用顺序竞争四槽；满槽请求不得在下一帧补建。已改为明确 RibbonTrail/RibbonColor 事件，更新只推进已持有实例。
- 原 ActivateRemovalPending 只对非光束保留附属效果；光束移除立即清除。新增边界验收：满槽拒绝后释放槽不自动补建、再次显式请求可成功、普通弹体残余排空、光束即时释放。

## 阶段六：地图粒子呈现与独立效果层容量补齐

证据：enemy_template.bt、particle_effect.bt；CEnemy native 31（72312–72340）按 3/4/5/6 个参数提供默认绘制组 3、朝向开关及 8.8 缩放，StartLinkedEffect 70563 和锚点 68586 保存并消费。真实 pack10 ENEMY 样本物理 0013（偏移 0x9CE）的 Flow @0x97 调用六参数，组 5、关闭朝向、缩放 128/256；资源下标 4 指向 pack5 PARTICLEEFFECT 106。CParticle::Spawn 133143 捕获绘制组，Draw 133554 消费播放器缩放；CParticleSystem::QueueParticles 133841 逐粒子进入 CRenderQueue，按组、整数 Y 排序。CEffectLayer::AddParticleEffect 66802 独立扫描 20 个槽，满时返回空。

方案：补传 native 参数和当前部件锚点；共享播放器支持缩放、出生时绘制组和单粒子提交；地图动态粒子进入现有对象排序队列，独立研究入口亦按组/Y 排序；CEffectLayer 固定 20 槽，不与地图槽合并。保留旧分层绘制调用接口，通过明确参数避免地图队列与研究绘制重复提交。

验收：六参数 Flow 值保持，关闭朝向不旋转；缩放只影响原绘制倍率，不改发射数和速度；不同绘制组/Y 顺序正确；独立效果层满后不扩容、不影响地图 20 槽；武器、敌人、场景物和死亡回归通过。

阶段六结果：Debug 三产物构建退出码 0（`obj/night-stage6-build.log`）；`weapon-effects,enemies,prop-combat,pickup-render,player-death,deathmatch-feedback` 6/6 通过，退出码 0，受保护资源变化 0。证据 `obj/night-stage6-evidence/`。阶段五附属效果补充修正的三项回归也已通过（`obj/night-stage5b-checks-fixed.log`）。

## 阶段七：枪械与手雷速度单位链

依据：gun_template.bt、bullet_template.bt；原 CGun resolver 128230–128267 将参数 4 乘 0x3B800000（1/256）；FireBullet 128140–128145 使用 0x43D70000 / 0xC3D70000（±430.0），按 sin/cos 生成世界速度；CBullet::Configure 62267 保存向量，UpdateDirect 62150–62181 以 0.001 × 毫秒积分。CBrother::ThrowGrenade 138729–138735 同样使用 ±430，没有枪械 Flow 倍率。实际 pack0_core GUN 物理 0009 @0x56FC 的 Flow @0x99/@0xBF 使用倍率 512，故初速为 860 世界单位/秒。

方案：枪械 450 改为已核实的 430；在 CGun 产生绝对初速，在 CBrother 手雷事件产生 430，CLevel 仅分配弹体并传递初速。不是资源表，也不改变 BIG。验收：真实默认枪械资源、native 倍率 0/128/256/512，在无碰撞、无加速度条件下 100ms 位移分别为 0/21.5/43/86；完整武器、地雷、手雷回归通过。

阶段七结果：Tests 构建退出码 0，`weapons,weapon-effects,mines,enemies` 4/4 通过（`obj/night-stage7-checks.log`），实际位移为 0/21.5/43/86，证据 `obj/night-stage7-evidence/`。枪械与手雷事件现在均传绝对世界速度，CLevel 不再持有 450/430 初速常量。

## 阶段八：相对粒子位置与动态朝向

补充证据：CParticleEffectPlayer 构造 131271–131272 默认循环、相对坐标；Init 131358 不覆盖坐标模式。CParticleSystem::AddEffect 133930–133932 显式非循环、世界坐标。ParticleEffectHolder 构造 295111 接受世界坐标标志；CBullet 两入口传 1。CBrother StartShield/StartFrenzyType 和构造保留相对模式；CParticle::Draw 133524–133535 为相对粒子加播放器当前位置，Update 133782 使用播放器当前角度，不是出生角度。

方案：共享播放器显式保存坐标模式，地图系统与弹体持有者启用世界坐标；强化和屏幕播放器保留相对坐标。绘制位置在同一播放器入口换算，发射算法不复制；每帧推进存量粒子前同步当前角度。原地图预览预放置播放器显式使用原有世界坐标方式，避免与其既有坐标适配重复平移。

验收：相对粒子随当前位置移动，世界粒子保持出生位置；动态旋转影响后续位移/绘制；停止、槽复用不泄漏；UI、强化、死亡及武器相关回归通过。

阶段八结果：Tests 构建退出码 0，`weapon-effects,powerup-play,refinery-menu,postgame-menu,postgame-presentation,player-death` 6/6 通过，退出码 0（`obj/night-stage8-checks.log`，证据 `obj/night-stage8-evidence/`）。补充修正 Ribbon 创建前的颜色请求不影响尚不存在的实例，与原 native 顺序一致。收尾增加角色退休边界：旧死亡爆裂保留归属与最后位置，链接循环停止发射，全部结束后才释放死亡完成查询。

## 迁移后的职责与运行边界

- **组合入口已删除。** `ZWeaponEffects.h/.cpp` 和 `ZShot` 均不存在于源码及测试引用；没有旧名别名、转发壳或同功能新组合类。`CLevel` 直接管理弹体生命周期，分文件 `CLevelEffects.cpp` 只调度对象和原事件；具体行为在 `CBullet`、粒子系统及效果持有者。
- **池与实例分开。** 地图 `CParticleSystem` 固定 20 个播放器、分配容量 200（可领取 199）；`CEffectLayer` 独立 20 个粒子效果槽，使用已有地图效果层池；弹体四槽容器与其粒子池是另一层限制。兄弟六个强化播放器共享角色 25 槽池，Powerup 五个屏幕播放器共享 100 槽池，菜单继续共享自己的池。没有把 UI、强化、预放置效果并入地图临时系统。
- **归属与锚点分开。** 死亡爆裂脱离旧角色锚点后继续保留死亡归属，防止重生换位置与结算提前结束。效果槽复用使用宿主代际句柄，旧引用不能停止新实例；这是 Windows 对象寿命适配，不是新的资源身份。
- **共享呈现。** `CParticleEffectPlayer` 负责自身粒子呈现；`ZSpriteRenderer` 只缓存／展开原 Sprite 并提交图块，复用 `CSpriteIterator`。地图动态粒子进入原有地图对象排序队列，保留出生时绘制组和整数 Y；预览投影由 `ZEffectProjection` 提供。
- **声音边界明确。** native 声音请求仍在 `CGun/CBullet/CBrother/CEnemy/CProp/CPowerup` 等原消费者产生；`ZCombatAudio` 保留现有每帧 WAV 合并、角色循环去重、移动声音忙碌窗口和模拟时钟，委托 `ZAudioPlayer`。未冒用原 `CSoundQueue` 名称，也没有把菜单纯粒子与声音重新绑定。
- **资源缓存不是数据源。** `ZParticleResources` 与 `ZBulletResources` 从同一 BIG 读取模板及依赖；`ZEffectColors` 是绘制时生成单色纹理的适配。没有人工 JSON/CSV 配置、资源 ID 白名单或原始资源改写。
- **正式与研究共用。** Game、Viewer、Tests 都调用 `CLevel/CBullet` 与同一粒子实现。调用方的成对 `effects/scene` 借用已合并；保留已有研究入口与 `CBrotherPowerups.cpp`。

## 未确认差异与下一阶段

本轮完成的是组合层消除及有证据的行为修正，不等于整款游戏已经逐方法对齐原版。

1. **优先核对弹体最终移除回调。** 原 `OnRemove` 62299 清空附属效果后通知 `CGun::OnBulletRemoved` 128523（调用枪械出口 2）；`ActivateRemovalPending` 62365、`UpdateRemovalPending` 63481、`ForceRemoval` 61876 涉及虚调用 +168/+172/+40。当前保留旧实现“开始退出时通知枪械，尾迹继续消散”的计数时点；尚未完成虚表及关卡回收调用链核对，不能宣称已与原版每帧时点一致。下一步连同 ForceRemoval 的同帧 Flow 重入、地雷计数和尾迹结束核对，避免仅把回调推迟后造成脚本停火。
2. **R08 仍未实施。** Viewer 的 `ZMapWorld::BuildInteractiveStates`、`ZMapParticles::StartTransitionParticlesForProp` 仍按场景物种类／状态选择转场；正式 `CProp` Flow 路径已经独立。应让手动研究输入驱动同一个原运行对象，核对原 CProp 出口与真实 PROP Flow 后删除旧表。
3. **预放置与其他效果分支。** `CParticleEffectProp` 完整类归属及独立池仍保留在现有地图适配中；`CEffectLayer` 本次恢复粒子分支，未恢复全部文字／Sprite 效果。地图动态粒子已逐粒排序，弹体仍沿既有玩家前／后两个绘制阶段，并非所有原对象已经进入统一渲染队列。
4. **其他已有差异。** 全局 Utility 随机序列与当前宿主随机状态未逐调用对齐；共享 UI 播放器不保证历史各组合对象初始种子的完全相同随机序列。`CParticle` 朝向／速度等其余参数，以及原 CBullet Update 中两次 Sprite 推进是否来自准确虚调用恢复，仍需独立核实。玩家／Bot 的 220 移动速度属于 R10 剩余项。
5. DM 玩家爆裂的已核实链仍是 PLAYER 导出 2 → Brother native 11 → 脚本资源 4 → core 类型 11／Section 12／序号 18，1690 字节、12 发射器、Sprite 原型 34；没有把它推广成所有敌人的统一效果。

## 最终验证与交接

### 命令与结果

| 检查 | 结果／退出码 | 日志与证据 |
|---|---|---|
| `pwsh -File obj/build-runtime.ps1 -Product Game` | Debug 三产物成功，0；最后增量构建无警告 | `obj/night-final-debug-build.log` |
| `pwsh -File obj/build-runtime.ps1 -Product Game -Configuration Release` | Release 三产物成功，0；有原有类型转换／符号比较警告 | `obj/night-final-release-build.log`，提取记录 `obj/night-release-warnings.txt` |
| Debug 集中回归，命令见下 | 21/21 通过，0 | `obj/night-final-debug-checks.log`、`obj/night-final-debug-evidence/` |
| Debug `weapon-effects,game-menu` 最终补验 | 2/2 通过，0；包含最后增加的退休归属边界和测试入口修正 | `obj/night-final-debug-boundaries.log`、`obj/night-final-debug-boundaries-evidence/` |
| Release `weapons,weapon-effects,mines,audio-transitions,player-death,powerup-play,refinery-menu` | 七项功能回归均通过，各项退出码 0 | `obj/night-final-release-checks.log`、`obj/night-final-release-initial-evidence/`；该首轮还包含下述已修复的冒烟失败 |
| Release `game-menu` 修正后补验 | 1/1 通过，0 | `obj/night-final-release-smoke.log`、`obj/night-final-release-smoke-evidence/` |
| `pwsh -File tests/verify-runtime.ps1` | 0；资源、其他工作目录、Viewer 启动、菜单、作弊码、测试账号保存、Release 拒绝研究参数全部通过 | `obj/night-final-runtime.log`、`obj/night-final-runtime-evidence/result.json` |
| Debug Viewer 武器实际预览，命令见下 | 开火、推进、截图后正常退出，0 | `obj/night-viewer-weapon.log`、`obj/night-viewer-weapon.png` |
| `git -c core.safecrlf=false diff --check` | 0；未提交 Git | `obj/night-diff-check.log` |

Debug 集中回归：

```powershell
pwsh -File tests/run.ps1 -Configuration Debug -Case debug-input,viewer-controls,weapons,weapon-effects,mines,audio-transitions,postgame-presentation,player-death,local-live,deathmatch,deathmatch-feedback,enemies,boss,pickups,prop-combat,pickup-render,profile-play,powerup-play,refinery-menu,postgame-menu,game-menu -NoBuild
```

最后补验与独立 Viewer：

```powershell
pwsh -File tests/run.ps1 -Configuration Release -Case game-menu -NoBuild
pwsh -File tests/run.ps1 -Configuration Debug -Case weapon-effects,game-menu -NoBuild
./bin/Debug/GunBrosViewer.exe --weapon 66 --fire --mute --config E:/coding_projects/c_projects/gun_bro_re/obj/night-viewer.cfg --screenshot E:/coding_projects/c_projects/gun_bro_re/obj/night-viewer-weapon.png --advance 1000
```

`tests/run.ps1` 自动给运行对象加 `--mute`。所有上述测试脚本的受保护 BIG、存档样本、用户账号和配置变化均为 0；没有机械运行全量截图基线。`verify-runtime.ps1` 使用测试账号，未改玩家存档。截图和每项 stdout/stderr 已复制进对应 `obj/night-*-evidence/`，避免后续测试清空 `tests/out` 后丢失证据；这些本地证据与构建产物由 Git 忽略。

### 验证中发现并修复的问题

- 第一阶段音频测试漏绑炼油厂模板，触发原有空指针访问；补齐测试准备过程，未改正式逻辑，详见阶段一。
- Release 首轮七项功能检查通过，但 `game-menu` 退出码 2：测试脚本无条件传入仅 Debug 支持的 `--screenshot/--menu-page`。现按配置选择参数，Release 通过已有 `[menu] ready` 日志确认初始化，再向本次 PID 下标题为 `Gun Bros` 的窗口发送正常关闭消息。`tests/ZRuntimeTestWindow.ps1` 是测试进程窗口适配，不进入产品代码。
- 等待就绪的诊断轮发现 `CopyToAsync` 输出文件缓冲会把最后的就绪标记留在内存。测试日志改为无文件缓冲，让运行中标记即时可见；诊断记录保留在 `obj/night-release-smoke-buffer-diagnostic.log` 与 `obj/night-release-smoke-buffer-evidence/`。修正后 Release 冒烟 1.2 秒、Debug 冒烟 3.8 秒通过。没有给 Release Game／Viewer 加回截图能力。
- 保留原注释含义，迁移后重新归位了拖尾、发射周期、绘制排序说明。R03 的旧外观假设注释保留并明确标注已否定，避免旧推断再次变成资源规则。

### 已查看的必要画面

- `obj/night-final-debug-evidence/OriginalUI/refinery-menu/refinery-coin-flight.png`：炼油厂资源转移过程中菜单仍正常呈现。
- `obj/night-final-debug-evidence/Core/deathmatch-feedback/deathmatch-final-burst.png`：最后一击保留玩家爆裂与残肢效果。
- `obj/night-final-debug-evidence/Core/weapon-effects/boss-beam-pack5-104.png`：主体直接使用原动画 1；图中周期亮节来自该绑定，没有按外观减一修补。
- `obj/night-viewer-weapon.png`：独立 Viewer 使用同一武器与附属效果实现实际开火。

这些画面和回归证明相关流程可运行，不代替逐像素或全部原类执行顺序的验证。下一阶段优先事项和未确认边界见上一节。
