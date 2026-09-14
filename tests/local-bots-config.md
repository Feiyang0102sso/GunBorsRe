# 本地机器人配置

新增或调整好友只需编辑一个 `local-bots.cfg`，不需要制作完整存档。当前实现复用原版账户系统，后台自动生成编号存档保存游戏中产生的经验、货币和库存；这些文件不属于手工配置入口。

游戏启动时读取当前玩家账户目录下的 `local-bots.cfg`。默认 Release 账户对应 `bin/Release/saves/local-bots.cfg`，Debug 对应 `bin/Debug/saves/local-bots.cfg`；使用 `--profile` 时跟随指定账户。不存在时，程序会从原有 LOCAL BOT 账户生成完整配置；没有旧 bot 时，首次复制当前玩家的装备和库存。

每个 `[身份]` 是一个好友。复制整段、换一个唯一身份和名字即可添加；启动时循环读取所有条目，新 Live 匹配使用 BROS 当前激活的 bot，REMATCH 保留本局队友；未激活 bot、选择原默认兄弟时，Live 使用配置中的第一位 bot。出战选择跨启动保存，重排文件不会改选中的身份。默认单人兄弟不计入好友数。

单人选好友只读取其装备、等级与外观，行为仍由原 `CBrotherAI` 执行。`BroAIDeathmatch` 仅在 Live 创建和运行。关闭 fake connection 后，菜单清除好友选择并恢复默认兄弟；冷启动时也执行该校正。默认兄弟名字按 BIG 显示 Percy Gun／Francis Gun，配置条目的自定义名字独立保留。

```ini
[windows-test-bot-1]
name=LOCAL BOT
level=100
brother=1
gun0=pack5:64
gun1=pack5:38
armor0=none
armor1=none
armor2=none
coins=1000
warbucks=0
powerup.pack5:0=5
```

这是可编辑示例，不是原版资源表。首次自动生成的配置会保留现有 bot 的实际等级和装备，请优先修改那个文件。

| 字段 | 含义 |
|---|---|
| section 身份 | 英文字母、数字、`-`、`_`，不可重复；也是独立账户目录名 |
| `name` | HUD、商店、结算及 BROS 列表显示名，可含空格 |
| `level` | 原 BIG 等级表中的等级，经验／生命由原表换算 |
| `brother` | 原兄弟索引 0 或 1，决定人物／头像 |
| `gun0`、`gun1` | 两把枪的 `包名:类型内序号` |
| `armor0`～`armor2` | 三件盔甲的原资源引用；`none` 表示空槽 |
| `coins`、`warbucks` | 可选，配置账户货币 |
| `powerup.包名:序号` | 可选，将该原道具库存设为右侧数量 |

装备序号来自 GUN／ARMOR／POWERUP 对应类型，不能填 STORE 商品序号。名称、价格、伤害、模型及动画仍从 BIG 读取；不存在的引用会报错，不替换成默认装备。配置允许 `#` 或 `;` 开头的整行注释。

未修改条目的有效配置时，启动保留其赚取的经验、货币、装备和消费后的库存。修改某条配置后，下一次启动重新应用**该条中明确列出的字段**。配置的 `level`、库存或货币字段因此可用于重置测试状态。省略的字段继续保留账户状态。

独立账户位于 `local-friends/<身份>/`，其中原格式编号存档存装备、经验和库存；`friend.txt` 保存 BROS 出战选择，`definition.txt` 记录已应用配置。旧 `match-next.txt` 轮换游标已不再读取或写入。它们都不是原 1006 好友礼物记录的扩展。

BRO BOOST 按唯一有效好友数量计算，等级和是否出战不影响档位。1 好友每日金币 +10%，3 好友经验 +10%，9 好友矿 +10%；10 好友解锁原版全部档位。具体原函数、原生初始化表及加成算法见 `local-roster-boost-research.md`。AI 只决定移动、瞄准、切枪、购物和用道具，公共战斗／奖励系统处理加成。
