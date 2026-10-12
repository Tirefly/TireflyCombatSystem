# Tasks: 冷却与消耗的类型层

> 对应 `PLN-R6.5-1` Task 0（Step 1~8）。**验证循环 = 实现 → 双配置 UBT 编译 → 装置取证 → 文档回写**
> （本仓 `AGENTS.md` 明令 Unreal 开发禁 TDD）。
>
> **★ 归档时 MUST 手写两个新能力的 `## Purpose`**：归档器为新建能力写的是 `TBD` 占位
> （`TOOLS-9` 已登记，本轮将是第 3~4 次命中）⇒ 漏写会让 `openspec validate --all --strict` 变 `failed`。

## 1. 类型层

- [x] 1.1 落 `Public/Def/TcsCooldownTypes.h`——`ETcsCooldownTiming`（四档）+ `FTcsCooldownTrack` + `FTcsCooldownPolicy`
- [x] 1.2 落 `Public/Def/TcsCostTypes.h`——`ETcsCostPolicy`（三档）+ `ETcsCostTiming`（四档）+ `FTcsCostConfig`
- [x] 1.3 落 `Public/Skill/TcsCooldownState.h`——`FTcsCooldownTrackState` + `FTcsCooldownGroupState`（非反射运行态）
- [x] 1.4 落 `Public/Param/TcsSkillParamKeys.h` + `Private/Param/TcsSkillParamKeys.cpp`——`Tag_TcsSkillParam_CooldownPct`

## 2. 字段回补

- [x] 2.1 `FTcsLearnedSkillEntry` 回补 `TArray<FTcsCooldownTrackState> CooldownTracks`（含"当初不能写"的回补记录）
- [x] 2.2 `FTcsSkillDefData` 新增 `FTcsCooldownPolicy Cooldown` + `FTcsCostConfig Cost` 两字段

## 3. 验证

- [x] 3.1 刷新项目文件（新增 `.h`/`.cpp` ⇒ `-projectfiles` 必做）
- [x] 3.2 Development Editor 编译 → `Result: Succeeded`，0 error / 0 warning
- [x] 3.3 Game Shipping 编译 → `Result: Succeeded`，0 error / 0 warning
- [x] 3.4 `UHT` 产物核实：`TcsCooldownTypes.gen.cpp` / `TcsCostTypes.gen.cpp` / `TcsSkillDefData.gen.cpp` 的
      `NewProp_*_MetaData[]` 含全部新字段（**列出来的才是真反射字段**——防"假字段"）

## 4. 收束（归 `PLN-R6.5-1` Task 6 统一执行）

- [ ] 4.1 归档本提案后**手写** `skill-cooldown` / `skill-cost` 的 `## Purpose`
- [ ] 4.2 `SPEC-04-skill` §11.3 实现名对照补 `ETcsCostPolicy` / `ETcsCooldownTiming` / `ETcsCostTiming` /
      `FTcsCooldownPolicy` / `FTcsCostConfig`（**MUST NOT 再引用 `ECostPolicy` 那个旧写法**）
