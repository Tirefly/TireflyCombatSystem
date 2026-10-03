# [SPEC-06-targeting](../spec/10-module-targeting.md) — TcsTargeting 目标选择层设计（v2 定稿）

- **文档 ID**：`SPEC-06-targeting`
- **类型**：SPEC / 模块规格
- **状态**：ACTIVE
- **权威范围**：TcsTargeting 选择器/过滤器策略与注入接口；理由住 LOG-02-effects D4-15
- **最后更新**：2026-10-03

- 日期：2026-09-02
- 状态：**v2 定稿**——策略模式取代枚举模式（用户拍板：枚举太具体、不像模组词汇；目标语义 100% 宿主本体论）；修订记录见文末
- 职责一句话：**目标选择的策略契约与默认实现——抽象接口 + 少量默认策略 + SelectTargets 步骤执行器；只选目标，不执行效果、不渲染指示器。**

## 1. 模块边界

- 消费者：M4（SelectTargets 步骤+执行器经注册表分派）、M5（技能链编排经链步骤间接消费）、宿主（策略实现/注入接口实现/指示器渲染）。
- 依赖：TcsCore（句柄/总线）、TcsEffect（FEffectStep 基类型 + EStepResult + `UE_DEFINE_EFFECT_STEP_EXECUTOR` 自注册宏；**ICombatEntityQuery 注入接口**——机制层定义，宿主实现；实现名 ITcsEntityQuery 见 plan2）。**不依赖 TcsState/TcsAttribute/TcsDamage/TcsSkill**——单位遍历/存活/阵营判定经注入接口（具名审计 MEM-20260902-13）。
- R3：六模块之一（计划二）——**策略接口 + Self/EventTarget 两默认实现 + SelectTargets 执行器**；RadiusArea/FTargetingShape 后置（用户拍板：竖切剧本单体选择已覆盖，无消费者不预设）。

## 2. 类型词汇（对外）

### 2.1 选择器策略（策略模式取代枚举模式，用户拍板）

> **证据状态（2026-09-28 增量）**：宿主选择器改用 `UPARAM(ref)` 后，生成 C# 签名为 `ref IList`；当前 PIE 已验证 selector 目标顺序 `[1,2]`、过滤器 AND 短路、空 selector Host 的空集 + Warning、空 filter Host 的全通过，并在原生 `Obj GC` 后再次调用脚本 selector/filter（新签名证据见 `evidence/2026-09-28-selector-ref-pie.md`；旧签名证据见 `evidence/2026-09-28-host-scripting-e2e-pie.md`）。实体世界查询仍是 C++ 专用，其他语言未独立实测。
- `FTcsTargetSelectorStrategy`（USTRUCT 反射基类，**纯虚 `Resolve(Context, 注入查询, OutTargets)`**——解析目标集写入 Context.Targets。**载体（D3-7 v3）**：USTRUCT 基类 + C++ 虚函数分派（StateTree 同构），Def/步骤以**裸 `FInstancedStruct`** 成员持有（2026-09-24 换型）；BP 策略扩展通道放弃（R0 §9"蓝图不承诺"承责）；宿主扩展 = **C++ 新 struct 子类（框架级通用语义）或宿主脚本插槽（客制化语义）**——见 §2.1 末条与台账 SCRIPT-8）。
- **默认实现（框架提供）**：
  - `FTcsSelSelf`——Context.Caster；
  - `FTcsSelEventTarget`——Context 事件载荷目标。
- **后置项**：RadiusArea（范围选择）与 FTargetingShape（形状单一来源）整体后置——竖切剧本（单体选择）无消费者；形态（裸半径 vs 形状参数化）待真实需求出现时再定（走查样例 04 §9 例二为预演形态）。TagQuery/Instigator 同批后置。
- **宿主扩展（2026-09-24 修正：两条路，按判据选）**：
  - **① C++ 新 struct 子类**——适合"框架级通用语义"（准星指向、链接目标这类可能被多个项目复用的）；零框架改动、走快路径（虚分派）。
  - **② 宿主脚本插槽（台账 SCRIPT-8，✅ 已落地）**——适合"客制化、只服务本项目业务"的语义。机制 = **宿主委托策略类型** `FTcsSelHostDelegate : FTcsTargetSelectorStrategy`（内部持 `TScriptInterface<ITcsTargetSelectorHost>` 转发给 UObject 实现）；宿主用**任意 UE 脚本语言**（C#/AS/Luau/TS/蓝图）实现该接口。**为什么必须有这条**：选择器族走**虚分派**，而 C# 定义的 struct **没有 C++ 类型 ⇒ 无 vtable ⇒ 野调用**（物理不可达）——插槽是绕过该约束的唯一路径。**先例**：`FTcsFlowDelegate` 步骤已持 `TScriptInterface<ITcsDamageFlowDelegate>`，形态现成。
    - **接口签名**：`ITcsTargetSelectorHost::ResolveTargets(FTcsCombatEntityHandle Caster, FTcsCombatEntityHandle Instigator, UPARAM(ref) TArray<FTcsCombatEntityHandle>& OutTargets)`（只填充不清空——同基类契约）；`BlueprintNativeEvent` ⇒ 形参 MUST 全反射 ⇒ **传句柄而非上下文**（上下文经门面按句柄访问器取，见 `04-module-effects.md` §5b）。
    - **C# `ref` 路径验证**：UHT/UnrealSharp 导出 `ref IList<FTcsCombatEntityHandle>`；LAC 探针追加目标后，PIE 验证原生回写与 GC 后重入（见 `evidence/2026-09-28-selector-ref-pie.md`）。原版 UnrealSharp 生成器重建、LAC 程序集重新发布和第二轮 PIE 已通过；仍未验证 C# 主动调用同名方法的反向回写，也未做全新克隆构建。
    - **空 Host 降级**：选择器产出**空集 + Warning**（不崩溃、不静默通过——空集与"配好了但选不中"在日志里可区分）。
    - **双轨并存**：`FTcsSelSelf` 等内置策略走虚分派快路径、原样保留；插槽只服务宿主扩展。
  - **判据**：框架内置 / 热路径 / 需引擎级不变式 → ①；客制化 / 只服务宿主业务 / 非热路径 → ②。
  - 配置面 = `FInstancedStruct` 内嵌编辑（StructUtilsEditor 开箱：类型 picker + 内嵌展开 + TArray 专用 details，非 TSubclassOf 下拉；2026-09-24 换型——`BaseStruct` 限定改由手写 metadata 提供）。
- **与 FEntrySelector 的边界（D4-4 纠正记录）**：`FEntrySelector` 是 M5 域——选择技能参数修正器作用到哪些已学条目；不是目标选择器。两个词汇不混用。

### 2.2 过滤器策略（存活/敌对语义 = 宿主本体论）
- `FTcsTargetFilterStrategy`（USTRUCT 反射基类，**纯虚 `Pass(Candidate, Context)`**——候选通过判定；载体同 D3-7 v3）。
- **框架零默认 Filter**：怎么算存活、怎么算敌人是宿主语义（原枚举 TTF_Alive/TTF_Hostile 是框架假装认识宿主语义再转手委托——v2 改为直接暴露契约）；R3 竖切由测试装置/宿主实现（内部可调 ICombatEntityQuery.IsAlive / IRelationResolver.IsHostile）。
- 组合语义：SelectTargets 步骤持 Filter 策略实例数组，**AND 全过**。
- **宿主脚本插槽（✅ 2026-09-24 落地）**：`ITcsTargetFilterHost::PassTarget(Candidate, Caster, Instigator)`（`BlueprintNativeEvent`）+ 转发器 `FTcsFilterHostDelegate`——同 §2.1 的②（虚分派物理不可达，故必须转发）。**空 Host 的语义 = 通过**（与基类中性默认实现同口径——"框架零默认 Filter"指不提供**有语义**的默认实现，而非把"未配置"变成"淘汰全部候选"；后者会让配置遗漏表现成"打不到人"，比放过更难排查）。
- **资格与分数的分工（2026-10-03 补，评分相位落地配套）**：**"谁有资格"归过滤器、"谁更优先"归评分器**——MUST NOT 用"给不合格候选打极低分"来实现淘汰。两条理由：①结果会随候选池的稀薄程度漂移（本该淘汰的候选在只剩它时会顶上来），而"淘汰"应是**候选自身属性**；②"极低分"与"真的分低/`NaN`"在数据上不可区分，排除与排序会在诊断面糊成一团。淘汰一律经 `FTcsTargetFilterStrategy::Pass`（数组 AND 全过）。

### 2.3 注入接口（宿主实现，M6 适配）
- `ICombatEntityQuery`（UINTerface，TcsEffect 定义——机制层对宿主能力的契约）：实体遍历（稳定序全量句柄）/ GetLocation / IsAlive。实现适配：军官组件 / Mass 桶（M6）——内部可转发中央注册表（M3 拥有；命名统一待办，R3 执行期定）。
- `IRelationResolver`：阵营关系判定——IsHostile / IsFriendly(来源, 目标)。
- 传递方式：经 FEffectContext 携带的注入引用访问（链上下文注入——运行时协作不产生编译边）。

### 2.4 评分器策略与排序项（2026-10-03 新增，排序相位落地）
- `FTcsTargetScorerStrategy`（USTRUCT 反射基类，**虚函数 `Score(Candidate, Context, EntityQuery) -> double`**，载体同 D3-7 v3）：**中性默认实现 = `0.0`**（"不区分候选"，口径同 §2.2 的空 Host——MUST NOT 把"未实现"变成"淘汰/垫底"）；`EntityQuery` **可空**，缺查询设施时降级值 MUST 为 `NaN`（不得用 `-Inf`/`0` 冒充——`NaN` 是唯一"不可比较"标记，见 §3 的排除规则）。
- `FTcsTargetSortItem{ FInstancedStruct Scorer; ETcsTargetSortDirection Direction; }`：排序项 = 评分器 + 每项**独立**方向（值 0 = 升序）。多项构成**严格字典序**（前者全等才看后者）。
- **内置实现只有一个**：`FTcsScorerDistance`——**真实欧氏距离**（不是平方距离：分数是会被诊断面展示的**可观测值**，未声明的变换会误导）；任一端无定位 ⇒ `NaN`。
- **宿主脚本插槽**：`ITcsTargetScorerHost::ScoreTarget(Candidate, Caster, Instigator)`（`BlueprintNativeEvent`）+ 转发器 `FTcsScorerHostDelegate`——同 §2.1 的②。**空 Host = `0.0` + Warning**（同 §2.2 空 Filter 的裁定理由）；**脚本返回 `NaN` ⇒ 该候选被排除**（视为"本次无法评分"）。
- **性能边界（SHOULD，无法强制）**：评分器在热路径上按"每个候选 × 每个排序项"求值一次（**预计算**，不在比较中现算——逐次比较现算会把调用放大到对数倍），故 SHOULD 优先用 C++ 内置/自写评分器，脚本插槽留给低频或候选数小的场合；实测热点落在脚本评分器时再考虑加限制（已登记台账）。

## 3. 入口服务与关键机制

- **SelectTargets 步骤 + 执行器**：`UE_DEFINE_EFFECT_STEP_EXECUTOR` 静态自注册（D4-14）；执行管线（2026-10-03 补排序相位后为五段）= **策略 Resolve → Filter 数组 AND 过滤 → 去重（键 = 句柄身份）→ 排序（排序项字典序，可为空）→ 取前 K（`MaxCount`，0 = 不限）→ 结果写 `FEffectContext.Targets`**（供后续战斗步骤消费）。排序项为空时**保持既有保序**——"保序"因而是**有条件**的措辞，唯一例外条件就是"配了排序项"。
- **目标集 = 链上显式数据流**：SelectTargets 产出 Context.Targets → 下游步骤消费；再次 SelectTargets 显式改写（顺序链天然支持"多效果各有目标"——每段前插 SelectTargets）。多次选择各自截断是**显式数据流语义**（不额外约束"累积截断"）。
- **Context 默认目标初始化（D4-4 v2 改口配套）**：Context 创建时 Targets 默认 = 事件载荷目标——灼烧 tick 这类单步链 `[Damage]` 零 SelectTargets 步骤直接消费；需要不同目标集时才显式插 SelectTargets。
- **确定性纪律（D0-1）**：遍历序 = 注册表稳定序（Index 升序），过滤保序——同输入同输出。**排序相位（2026-10-03 补）**：配了排序项时按**严格字典序**逐项比较（每项独立方向），全等时以**稳定键 `FTcsCombatEntityHandle::Id` 升序**决胜（该句柄 Id 单调递增且**永不复用** ⇒ 不存在"回收槽误命中"，天然是稳定序）；评分为 `NaN` 的候选**被排除**（既不参与排序、也不出现在结果里，**不是排到末尾**）；去重 MUST 发生在**取前 K 之前**（去重后少于 K 才是真少于 K）。
- **确定性口径 = "同进程"（2026-10-03 更正）**：本条原写"客户端重算一致"——该**跨进程**承诺既不成立也不必要：`Id` 由**进程内**原子发号器分配（两台机器对同一批实体得到不同的 Id 序），且 §4 已定"仅权威侧执行、选择结果不复制"。故本模块的承诺是**同进程内同输入同输出**；跨端一致性由**复制的操作流与事件**保证，不由重算保证。
- **挂起语义**：SelectTargets 是即时步骤（不挂起）；策略/过滤产出的句柄经代际校验，悬空跳过。

## 4. 网络姿态落点（NET-1/2）

- 仅权威侧执行（04 NET：链执行权威侧全量，客户端镜像只跑 Cue 类步骤）；选择结果**不复制**——客户端表现由复制的操作流与事件驱动，归宿主。

## 5. 非目标

不做指示器/瞄准渲染（宿主/LAC，D4-15）；不做锁定/软锁辅助（玩家输入域归宿主）；不做空间加速结构（未来增量）；不认识具体效果与伤害（纯选择，域不透明）；不做投射物/区域实体（D4-16 移除）；**不做枚举式内置模式集**（v2 策略化——目标语义全宿主化，框架只立契约与少量默认实现）；**不做 Top-K 的流式实现**（2026-10-03：数据本就在手上，流式是 `O(H×K×M)` 的另一种取舍，仅凭"流式"之名不能推断更快）；不做候选来源组合器（交并差；触发条件 = 复合表达需求出现）。

## 6. 依据

- 拍板：D4-4（数据化+EntrySelector 纠正）、D4-14（注册制分派）、D4-15（模块成立）、D4-16（Spawn 移除）；**策略化三拍板（2026-09-02 审阅轮 5 后讨论）**——①Selector/Filter 策略模式取代枚举（用户："枚举太具体、不像模组该有的样子"；Filter"需要接口让宿主实现，不然没法确认怎么算存活怎么算敌人"）；②解法 B——TcsDamage 不内嵌 selector、读 Context.Targets，Context 默认目标=事件目标（编译集 {Core,Attribute,Effect} 保持，TcsDamage→TcsTargeting 边消失）；③RadiusArea/FTargetingShape 后置（竖切无消费者）。
- 证据：04 §2.3 / §9 例二（预演形态）/ §10 伪代码 D；TCS 09 核验（净新增）；R0 §9 模块表；竖切剧本 v2（单体选择，RadiusArea 无消费者实证）。
- 载体修订（D3-7 v3，2026-09-10 用户拍板）：策略载体 EditInlineNew Instanced UObject → FInstancedStruct/TInstancedStruct——依据 = UE5.8 StructUtils 已迁入 CoreUObject（编辑器开箱 picker/内嵌编辑/TArray details）+ StateTree"USTRUCT 策略基类虚函数 + FInstancedStruct 存储"shipped 先例 + 与 D4-14"数据+分派"心智收敛 + 网络开箱（原生 NetSerialize + Iris FInstancedStructNetSerializer）+ 旧 TCS"TSubclassOf 选类+FInstancedStruct 传配置"双形态不回归。

## 7. 修订记录

- v1（2026-09-02）：初版折入（R3 补文档轮）——枚举模式形态（3+2 收窄）。
- v2（2026-09-02，审阅轮 5 后讨论）：**策略模式重写**——用户三拍板（策略化/解法 B/RadiusArea 后置）；新增 Context 默认目标初始化；D4-4"战斗步骤内嵌"改口。
- v3（2026-09-10，D3-7 v3）：**策略载体切换**——EditInlineNew Instanced UObject → USTRUCT 反射基类 + C++ 虚函数分派 + `TInstancedStruct<Base>` 持有（可行性调研后用户拍板；类型名 UTcs→FTcs）；BP 策略扩展通道明确放弃（R0"蓝图不承诺"承责）；编辑器校验钩子由 Def 资产 IsDataValid 承担。
- v4（2026-10-03，排序相位 P-A 落地）：新增 §2.4 评分器策略与排序项（含唯一内置距离评分器、宿主脚本插槽、性能边界）；§2.2 补"资格不得伪装成低分"的分工纪律；§3 执行管线改写为五段（Resolve → 过滤 → 去重 → 排序 → 取前 K）并补排序契约（严格字典序 + 稳定键 `Id` 升序 + `NaN` 排除 + 去重先于 K）；**§5 删去"不做目标评分/权重排序（未见需求）"**（该非目标已被本相位取代）；§3 确定性口径由"客户端重算一致"**收窄为同进程**（依据见该条）。载体/依赖面零变化。

## 8. 验收钩子

计划二竖切：测试链 `WaitDelay → SelectTargets(单体策略) → Damage`——策略 Resolve → Context.Targets → Damage 消费；人工检查：策略内嵌编辑可用、过滤 AND 生效、单步链（无 SelectTargets）经 Context 默认目标直接命中、同帧重复选择结果一致（确定性）。
