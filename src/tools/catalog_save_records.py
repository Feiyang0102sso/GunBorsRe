"""只读解析全部当前存档，验证原封装公式、CRC与哈希，生成字段偏移索引。"""
from pathlib import Path
import hashlib
import json
import struct

from catalog_game_entries import Reader


ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / "out/binary-research/save-catalog.json"
COLLECTION_IDS = {1002: 10, 1003: 524, 1004: 8, 1005: 14, 1013: 14, 1016: 14, 1018: 14}
STATISTICS = (
    ("PLAY_TIME_TOTAL", "累计游戏时间；计时单位需由累加调用确认"),
    ("PLAY_TIME_SESSION", "原枚举SESSION时间槽；尚未找到累加消费者，单位及是否历史最大值未证实"),
    ("KILLS_TOTAL", "累计击杀数；结算IncrementStat :75775"), ("KILLS_SESSION", "单局击杀历史最大值，SetStatGreater :75776；不能只按SESSION字面称当前局"),
    ("WAVES_TOTAL", "累计波次数；:75935"), ("WAVES_SESSION", "单局波数历史最大值，SetStatGreater :75936"),
    ("SHOTS_TOTAL", "原枚举累计射击槽；尚未找到直接累加消费者"), ("SHOTS_SESSION", "原枚举SESSION射击槽；是否历史最大值未证实，不能仅凭名字称当前局"),
    ("PERFECT_WAVE_TOTAL", "累计完美波次数；:192874按未曾记入的完美波增加"), ("PERFECT_WAVE_SESSION", "单局完美波数历史最大值，SetStatGreater :76078"),
    ("NUM_PURCHASED_GUNS", "付费取得枪的次数；AcquireItem :158131—158142要求至少一种价格非0，每次加1，不等于当前库存数量"), ("NUM_PURCHASED_ARMORS", "付费取得盔甲次数；:158145—158148按STORE分类选择统计11"),
    ("NUM_PURCHASED_POWERUPS", "付费取得强化次数；AcquireItem :158142同一IncrementStat分支，不按本次批量quantity相乘"),
    ("DAMAGE_WITH_GREENSHIELD", "一次Green Shield期间挡下伤害的历史最大值；:136774在不扣HP分支传入护甲减伤后的伤害，:223859累加，:224630取最大值"),
    ("KILLS_WITH_SHOCKAWESOME", "一次Shockawesome使用期间击杀最大值；PowerupUseEnd :224651"), ("KILLS_WITH_FRENZY", "一次Frenzy期间击杀最大值；OnStopFrenzy :224640"),
    ("KILLS_WITH_PISTOL", "手枪击杀数"), ("KILLS_WITH_RIFLE", "步枪击杀数"),
    ("KILLS_WITH_SHOTGUN", "霰弹枪击杀数"), ("KILLS_WITH_SPREAD", "Spread类枪击杀数"),
    ("KILLS_WITH_HEAVY", "重武器击杀数"), ("KILLS_WITH_SPECIAL", "特殊武器击杀数"),
    ("HIGHEST_COMPLETED_REV", "最高完成轮次；CheckCompletedRevolutions :224763将关卡波次进度除每轮波数后SetStatGreater"), ("MOST_COINS_SAVED", "金币历史最大值统计；:193435调用的统计形参是u32，仅取64位金币余额低32位，不能替代1000的u64余额"),
    ("WAVES_USED_MULTIPLE_GUNS", "使用多把枪的波次数；:101037"), ("LONGEST_STREAK", "最大连续击杀数；CGame::UpdatePostGameStats :75901取GetBestKillStreak并保留最大值"),
    ("HIGHEST_COMPLETED_ZOMBIE_REVOLUTION", "最高完成僵尸轮次；CheckCompletedRevolutions :224758—224766取waveProgress/每轮波数并保留最大值"),
    ("HIGHEST_SIMULTANEOUS_ZOMBIE_NUKE_KILLS", "核爆同时击杀僵尸的最大值"),
    ("HIGHEST_SCORE", "历史最高分"), ("BRO_CHALLENGES_COMPLETED", "完成Bro挑战数"),
    ("BRO_BUFFS", "当前满足门槛的好友增益条目数；CFriendPowerManager::CalculateAggregates :234475—234495从0重新计数后SetStat，非累计使用次数"), ("CRITICAL_HITS", "暴击次数；:71573每次IncrementStat(31,1)"),
    ("DAILY_BONUSES", "领取每日奖励次数；CommitBonus :209575增加1"), ("AUTOAIM_USES", "原枚举名AUTOAIM；已找到实际消费者StartAutoFire :137234每次增加1，不能只凭英文解释成移动瞄准次数"),
    ("GIFTS_FROM_FRIENDS", "处理好友赠送XP的次数；ProcessPlayerXPFromFriend :199835增加1，并非所有商业礼包数"), ("PSTC_KILLS_WITH_BEAM", "Beam光束击杀数；原字符串含PSTC前缀"),
    ("MOST_BUCKS_SAVED", "持有War Bucks历史最大值"), ("DEATHMATCH_KILLS", "死亡竞赛击杀数"),
    ("DEATHMATCH_WINS", "死亡竞赛胜场"), ("DEATHMATCH_LOSSES", "死亡竞赛败场"),
    ("DEATHMATCH_PLAYS", "死亡竞赛参赛次数"), ("DEATHMATCH_CONSECUTIVE_WINS", "死亡竞赛连胜数"),
    ("BABES_SAVED_FLAGS", "救出角色位图；CLevel :118161调用SetStatBit，不能解释为人数"),
    ("JUNGLE_WAVES_CLEARED", "指定丛林关卡最大波次进度；:224768—224773限定packHash=23549986且LEVEL ordinal=0，使用SetStatGreater而非累加"),
    ("LOTTERY_A", "Lottery类型0进入结果状态4的记录；SetState :398045—398048只SetStatGreater(...,1)，不是抽奖累计次数"),
    ("LOTTERY_B", "Lottery类型1进入结果状态4的记录；:398042—398048只与1取较大值"),
    ("LOTTERY_C", "Lottery类型2进入结果状态4的记录；:398039—398048只与1取较大值"),
)


def crc_bzip2(data):
    """原CCrc32使用非反射CRC，不能用zlib.crc32替代。"""
    result = 0xFFFFFFFF
    for value in data:
        result ^= value << 24
        for bit in range(8):
            if result & 0x80000000:
                result = ((result << 1) ^ 0x04C11DB7) & 0xFFFFFFFF
            else:
                result = (result << 1) & 0xFFFFFFFF
    return result ^ 0xFFFFFFFF


def saved_key(reader, name):
    reader.read(name + ".packHash", "I")
    reader.read(name + ".objectOrdinal", "B")
    reader.read(name + ".objectType", "B")
    reader.read(name + ".syncState", "B")
    reader.read(name + ".alignment", "B")


def parse_payload(reader, store_id, version):
    """按各原SaveToDisk的逐字段/整块写出结构消费，不从随机填充推测负载终点。"""
    if store_id in COLLECTION_IDS:
        count = reader.read("recordCount", "I")
        for index in range(count):
            name = f"records[{index}]"
            saved_key(reader, name)
            if store_id == 1002:
                reader.read(name + ".quantity", "B")
                reader.raw(name + ".alignment", 1)
            elif store_id == 1003:
                reader.read(name + ".waveProgress", "H")
                reader.read(name + ".maxPerfectWaveIndex", "H")
                reader.raw(name + ".perfectWaveBits", 512)
            elif store_id != 1004:
                reader.raw(name + ".alignment", 2)
                reader.read(name + ".value", "I")
    elif store_id == 1000:
        reader.read("firstLaunch", "B")
        reader.raw("alignment1", 3)
        reader.read("xplodium", "Q")
        reader.read("coins", "Q")
        reader.read("warBucks", "I")
        reader.read("totalExperience", "Q")
        reader.read("playerLevel", "H")
        reader.raw("alignment2", 2)
        reader.read("xpBonusTimestamp", "I")
        reader.read("xpBonusQuantity", "I")
        for name in ("serverDataComplete", "pendingIAP", "pushChallenges", "reserved"):
            reader.read(name, "B")
    elif store_id == 1001:
        for name, count in (("guns", 2), ("bullets", 2), ("armors", 4)):
            for index in range(count):
                prefix = f"{name}[{index}]"
                reader.read(prefix + ".packHash", "I")
                reader.read(prefix + ".cachedPackIndex", "H")
                reader.read(prefix + ".objectOrdinal", "B")
                reader.raw(prefix + ".alignment", 1)
        reader.read("activeWeaponSlot", "B")
        reader.read("character", "B")
        reader.raw("alignment66", 2)
        for name in ("gunMastery0", "gunMastery1", "activeGunMastery0", "activeGunMastery1"):
            reader.read(name, "I")
        for index in range(8):
            reader.read(f"configuration96Raw[{index}]", "I")
        reader.read("selection128Raw", "B")
        reader.read("selection129Raw", "B")
        reader.raw("alignment118", 2)
    elif store_id == 1006:
        reader.read("nextGiftWriteSlot", "H")
        count = reader.read("giftRecordCount", "H")
        for index in range(count):
            reader.read(f"gifts[{index}].friendClientId", "i")
            reader.read(f"gifts[{index}].experienceGift", "I")
    elif store_id == 1007:
        for index in range(22):
            reader.read(f"tutorialSeen[{index}]", "B")
    elif store_id == 1008:
        if version != 1:
            raise ValueError("Refinement schema requires version 1")
        reader.read("checkpointTimeSeconds", "I")
        for index in range(12):
            prefix = f"slots[{index}]"
            for name, kind in (("state", "i"), ("efficiency", "f"), ("remainingTimeMs", "i"),
                               ("startTimeSeconds", "I"), ("totalDurationMs", "i"), ("xplodiumAmount", "Q")):
                reader.read(prefix + "." + name, kind)
    elif store_id == 1009:
        for name in ("lastLaunch", "consecutive", "lastCommit"):
            reader.read(name, "I")
    elif store_id == 1010:
        count = reader.read("statisticCount", "I")
        for index in range(count):
            reader.read(STATISTICS[index][0], "I")
    elif store_id == 1011:
        reader.read("friendMax", "I")
    elif store_id == 1012:
        for name in ("tapjoyPointTotal", "bootCount", "gameToStoreCount", "dontShowAgain"):
            reader.read(name, "I")
        reader.raw("dontShowStoreIdUtf32", 1024)
        reader.read("facebookAccountLinked", "I")
        reader.read("inviteFriendsIncentiveCount", "I")
        reader.read("facebookLoginAttempts", "B")
        reader.read("playhavenLaunchCount", "B")
        reader.raw("alignment", 2)
    elif store_id == 1014:
        count = reader.read("packCount", "B")
        for index in range(count):
            prefix = f"packs[{index}]"
            reader.read(prefix + ".packHash", "I")
            size = reader.read(prefix + ".byteLength", "I")
            start = reader.position
            categories = reader.read(prefix + ".categoryCount", "B")
            for category in range(categories):
                name = f"{prefix}.categories[{category}]"
                reader.read(name + ".category", "B")
                bits = reader.read(name + ".bitCount", "B")
                reader.raw(name + ".seenBits", (bits + 7) // 8)
            if reader.position - start != size:
                raise ValueError(f"Content pack length mismatch: {prefix}")
    elif store_id == 1017:
        if version != 5:
            raise ValueError("Challenge schema requires version 5")
        reader.read("lastUpdate", "I")
        reader.read("newChallenge", "B")
        reader.read("newRequest", "B")
        for name in ("playerProgress", "rewardStatus"):
            count = reader.read(name + ".count", "B")
            for index in range(count):
                reader.read(f"{name}[{index}]", "B")
        count = reader.read("friendGroupCount", "B")
        for index in range(count):
            records = reader.read(f"groups[{index}].count", "B")
            for record in range(records):
                name = f"groups[{index}].friends[{record}]"
                reader.read(name + ".clientId", "i")
                for field in ("progress", "requestedByPlayer", "incomingRequestFlag", "alignment"):
                    reader.read(name + "." + field, "B")
        count = reader.read("counterCount", "B")
        for index in range(count):
            name = f"counters[{index}]"
            reader.read(name + ".circumstanceKills", "H")
            reader.raw(name + ".alignment", 2)
            for field in ("clearedWavesBits", "perfectWaves"):
                reader.read(name + "." + field, "I")
            reader.read(name + ".initialPlayerLevel", "H")
            reader.read(name + ".initialFriendCount", "H")
            reader.read(name + ".usedPowerupsBits", "I")
    else:
        raise ValueError(f"Unknown store ID: {store_id}")


def write_statistics_template():
    lines = [
        "// 原CPlayerStatistics::SaveToDisk :221115：u32 count，随后count个u32；当前GetSaveSize :220165为192。",
        "// 每项名字逐字来自原PlayerStatCategoryStrings :29170–29220；末尾NUM_PSTC/INVALID不是存档项。",
        "// SetStatBit :220857 / SetStatGreater :220888 / SetStat :220907分别处理位图、历史最大值和普通值。",
        "typedef struct {", "    uint32 statisticCount; // 当前47；以下按iOS3.6.0固定枚举展示，变更版本须重新核对。",
    ]
    for index, (name, description) in enumerate(STATISTICS):
        lines.append(f"    uint32 {name}; // [{index}] payload+0x{4 + index * 4:X} / mem+{8 + index * 4}：{description}。")
    lines.append("} PlayerStatisticsPayload;")
    path = ROOT / "_Big_tool/binary template/saves/save_statistics.bt"
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main():
    baseline = json.loads((OUTPUT.parent / "saves-before.json").read_text(encoding="utf-8"))
    records = []
    for path in sorted((ROOT / "saves").iterdir()):
        if not path.is_file():
            continue
        data = path.read_bytes()
        digest = hashlib.sha256(data).hexdigest()
        record = {"path": path.relative_to(ROOT).as_posix(), "bytes": len(data), "sha256": digest}
        reader = Reader(data)
        if path.name.endswith("_PDST"):
            for index in range(19):
                reader.read(f"dataStoreStatus[{1000 + index}]", "B")
            if reader.position != len(data):
                raise ValueError("Unexpected PDST size")
        else:
            store_id = int(path.name.split("_")[-1].split(".")[0])
            version = reader.read("version", "i")
            mode = reader.read("wrapperMode", "I")
            if mode != 0:
                raise ValueError("Legacy wrapper needs separate validation")
            prefix = reader.read("prefixSize", "I")
            reader.raw("randomPrefix", prefix)
            start = reader.position
            parse_payload(reader, store_id, version)
            size = reader.position - start
            aligned = size + 512 - size % 512
            if len(data) != aligned + 24 or prefix != (aligned >> 1) - (size >> 1):
                raise ValueError(f"Envelope size mismatch: {path.name}, payload={size}")
            reader.raw("randomSuffix", len(data) - reader.position - 8)
            reader.read("ownerClientId", "i")
            actual = reader.read("crc32Bzip2", "I")
            expected = crc_bzip2(data[:-4])
            if actual != expected:
                raise ValueError(f"CRC mismatch: {path.name}")
            record.update({"store_id": store_id, "version": version, "payload_offset": start,
                           "payload_bytes": size, "crc_valid": True})
        record["fields"] = reader.fields
        records.append(record)
        print(f"Parsed {path.name}: file={len(data)} payload={record.get('payload_bytes', 19)}")
    # 原件哈希比对单独保留结果；存档从未写回。
    baseline_by_path = {}
    for entry in baseline:
        baseline_by_path[entry["path"]] = entry["sha256"].lower()
    for record in records:
        if baseline_by_path[record["path"]] != record["sha256"]:
            raise ValueError(f"Original changed: {record['path']}")
    OUTPUT.write_text(json.dumps({"original_hashes_unchanged": True, "files": records}, ensure_ascii=False, indent=2), encoding="utf-8")
    write_statistics_template()
    print(f"Verified {len(records)} original hashes; wrote {OUTPUT}")


if __name__ == "__main__":
    main()
