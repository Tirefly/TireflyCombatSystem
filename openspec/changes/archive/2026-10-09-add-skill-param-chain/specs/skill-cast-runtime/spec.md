## MODIFIED Requirements

### Requirement: 激活期参数快照绑定

激活成功后 MUST **在门禁全过之后、任何后续读取之前**构建并绑定一份参数快照——它是"施法期间读到的数值不再漂移"的唯一机制：

- MUST 提供 `BuildSkillSnapshot`：按 `FTcsSkillDefData.Params` **逐行**处理并冻结进 `FTcsCastRun.ParamSnapshot`；**写入点规则 MUST 与状态侧逐字同款**（覆盖值优先且不再过值约定 / 否则求值后按该行 `ValueConvention` 转规范值 / 重建语义 = 先 `Reset` 再填）；
- **`Mode` 列 MUST 参与分流（D5-12 v2 的"逐参数 Mode"）**：`EPM_Snapshot`（默认）⇒ **求值并冻结**进快照；`EPM_Live` ⇒ **标记跳过**（不进快照的数值面，实时读取走账本求值）。**MUST NOT 无差别地把 `EPM_Live` 行也冻结**——那会让"实时通道"名存实亡（配了 Live 的行在施法期读到的仍是冻结值）；
- **快照的消费面 MUST 是"求值期的参数表载体"（2026-10-09 用户裁定 A′，本条是防误读的唯一说明）**：快照 MUST 作为**物化/求值时的参数表**被绑定（经既有读取适配器 `UTcsStateParamTableReader` + `FTcsStateSnapshotScope`），供引用类数值源（`ParamRef`）取值——**先例 = 状态侧的 `FTcsStateModifierMaterializer` 把 `Instance.ParamSnapshot` 装进 `Ctx.ParamTable`**。技能侧的落实点 = `FTcsCastOps::Activate` 物化 `ParamChainRows` 时的那段绑定作用域。**MUST NOT 把本快照读成"公共读口的冻结来源"**：`GetNumericParam` 走**实时**通道（参数行初值 + 账本槽位折叠），**MUST NOT** 被要求改读快照——判据：快照住**每次 run**（`FTcsCastRun.ParamSnapshot`），而读口签名 `(Unit, EntryHandle, Key)` **没有 run 段**，且 `CI_InstancePerExecution` 下同一 Entry **可有两个 run 并存** ⇒ "读哪一次施法的冻结值"**无唯一答案**；为一个无唯一答案的读数给读口加 run 形参 = 零消费者预建（施法期确需"某次 run 的冻结读数"时按台账另议读口）；
- **缺口登记（2026-10-09 实测，MUST 记录以防"以为已实现"）**：本能力的初版**只构建了快照、没有任何消费者**，且 `Mode` 列**完全未被读取**（`BuildSkillSnapshot` 无 `Mode` 判断 ⇒ `EPM_Live` 行也被冻结、"实时通道"名存实亡）。本变更即其闭合：**补上 `Mode` 分流 + 快照作为求值参数表被真正消费**（快照从"只写不读"变为"激活期物化链行的求值表"）。**同批如实订正一处初版误记**：初版曾把缺口写成"`GetNumericParam` MUST 改走快照"，该措辞**与 A′ 裁定冲突且本身不成立**（读口无 run 段、无唯一答案）——正确缺口是"快照零消费者 + `Mode` 未接"，不是"读口未接快照"；
- **复用面与不复用面 MUST 分清**（防后人误读为"建了第二套快照"）：**快照类型** `FTcsParamSnapshot` 与**读取适配器** `FTcsStateParamTableReader` + `FTcsStateSnapshotScope` MUST **复用**，MUST NOT 新建第二套快照类型、也 MUST NOT 出现第二处键查找语义；但**构建函数** MUST 另有一份——状态侧的 `FTcsStateOps::BuildSnapshot` 形参硬编码为 `const FTcsBuffDef&`，技能侧**物理上无法复用**。两份构建函数 MUST 保持同一套写入点规则（判据 = 规则同款，而非函数同一）；
- **时序 MUST 为**：门禁全过 → 构建快照 → 绑定到新建的 run → 起主链 → 发 `OnCastStarted`。**MUST NOT** 在门禁之前构建（门禁失败即白算一份快照）；
- `Level` MUST 取 **激活时快照的生效等级**（`EffectiveLevel` 语义 = `clamp(0, LevelBase + Σ参数账本 Level 键修正)`；本轮 `Σ` 由 Task 4 的参数链提供，本 Task 取 `LevelBase` 与条目的持久等级口径）——**运行中升级不追溯**；
- 快照 MUST 随 run 存活（run 终结时随池槽位归还而失效），MUST NOT 跨 run 共享一份快照。

#### Scenario: 施法中途改数值来源不影响本次读数

- **WHEN** 激活一个含参数行的技能，随后改变该行数值来源在别处会读到的输入
- **THEN** 本次 run 的快照内该键保持激活时刻求值得到的值（run 存续期内不重算）

#### Scenario: 快照写入点的值约定只转一次

- **WHEN** 某行配 `VCF_Percent` 且书写值为 85，激活该技能
- **THEN** run 快照内该键为 `0.85`（转换发生一次，读快照不再转）
