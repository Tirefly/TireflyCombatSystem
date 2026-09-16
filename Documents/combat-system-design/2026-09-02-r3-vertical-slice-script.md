# R3 竖切验收剧本 v2（模块构成 A' 定稿版，2026-09-02 M9 收尾轮重写）

- 日期：2026-09-02（v2 重写；v1 成文于 M4/M9 重构前，四处被打旧已修正）
- 目的：方案 C 的第一个可运行里程碑——"一条伤害链"定义到可勾选粒度；R3 纯 C++，禁 TDD，验收 = 编译通过 + 下列人工检查单。
- 证据/假设标注：工时口径为**当前假设**（粗估），非承诺；验证点为验收规格。

## 模块构成（A' 定稿：六模块 + TcsNotation 骨壳）

**进入 R3**：`TcsCore / TcsNotation（骨架壳——D5-18 v2 依赖边编译前提）/ TcsAttribute / TcsEffect（解释器+执行器注册表）/ TcsTargeting（策略接口+单体选择器）/ TcsDamage（默认模板+标准步骤库）/ TcsIntegration（CombatEntity 接线+PIE 地图）`

**不进 R3**：`TcsState`（M3）、`TcsSkill`（M5）、`TcsCue`（M7——用户 M9 收尾轮拍板移出：验收信号改走 D0-6 屏显通道，ICuePresenter 契约留 M7 轮）、`TcsEditor`（M8）

## 范围内

**固定装置**
- 1 个 PIE 测试地图；2 个测试单位（均走 `UTcsCombatEntityComponent` 军官组件路径；**Mass 不进竖切**）
- 属性表 4 行：Health / MaxHealth / Attack / Armor（**FName 词表 + 显式包装结构** + DataTable 行，验 D2-1 终定；~~强类型 UEnum~~ 已否决）
- 1 条测试链：`WaitDelay(0.5s) → SelectTargets(单体) → Damage`（手动 API 触发，**不做触发行**）
  - Damage 步骤走 TcsDamage **官方默认模板**（本测试可裁剪 Hit/Crit/Element 段：CollectStart → BaseDamage → Execute → Completed），公式 = 测试项目 delegate（`max(1, Attack聚合值 − Armor聚合值)`）
  - WaitDelay 步骤使链挂起到期堆——顺带验挂起-恢复协议与时钟泵（为检查点 7 提供消费者）
- **观测口**：日志归 UE 原生分类制（每模块 `LogTcs<模块名>`，运行期 `log LogTcsXxx Verbose` 可调）；屏显验收信号 = 测试装置直调 UE 屏显 API（AddOnScreenDebugMessage）——TcsCore 零日志设施（D0-6 最终收缩，用户拍板：统一注册入口无必要）

**链路**
手动触发 API（`UTcsCombatEntityComponent` 直调链执行）→ WaitDelay 挂起 → 到期唤醒 → SelectTargets（单体）→ Damage 流程（默认模板：读公式参数初值 → delegate 公式 → 流程属性黑板 → M2 事务扣血 → 提交尾行内 flush）→ **屏显/Output Log 可见**（Health 变更 + 流程记录——测试装置直调 UE 屏显 API + `LogTcsDamage` 分类日志）。

**修改器来源**：测试 Source 直接挂属性账本（**ApplyTestModifier 式 API** + Source 句柄——plan2 组件门面）——M3/TcsState 不在，无 buff。

## 验证点（人工检查单，7 项）

1. **数值正确**：`damage = max(1, Attack聚合值 − Armor聚合值)`；Health 被 clamp 到 0（验 D2-4/D2-5）。
2. **单次重算**：一个测试 Source 挂 2 个 modifier（Attack+10 / Armor×1.5），只触发 1 次重算 + 1 次变更广播；Source 注销（`RemoveBySource`）后恢复（验 D2-2/D2-5；~~buff 挂 2 modifier~~ M3 不在，改测试 Source）。
3. **flush 时序**：提交尾行内广播生效；帧末安全点默认关（验 D2-5）。
4. **句柄悬空**：实例移除后用旧句柄访问触发 ensure（验 D0-2 代际校验）。
5. **依赖铁律 grep**：无反向 include；六模块依赖边 = R0 §9 表（表列依赖=最小编译集）；实例代码文件无 `TSubclassOf`（验铁律 1/3 + MEM-20260902-13 两套语义审计）。
6. **数据驱动线**：新增一条只改数值的链 = **纯数据资产（链/流程配置），零 C++**（验铁律 5；~~新增 buff~~ M3 不在，改验链数据——同一数据驱动原则，对象换成链）。
7. **ScaledDt**：World 暂停/降速时 WaitDelay 的到期队列同步冻结/减速（验 D0-1/D0-5；消费者 = 测试链的 WaitDelay 步骤）。

**日志附加检查**：UE 原生分类控制生效（`log LogTcsDamage Verbose` 开启流程追踪、恢复默认）；屏显仅由测试装置产生（插件模块零屏显调用）。

## 范围外（显式非目标）

无 UI、无 AI/StateTree 集成、无 Mass 适配、无网络、无触发行、无 buff/状态机（M3）、无技能/施法/冷却（M5）、无 Cue/表现契约（M7）、无编辑器工具（M8）、无 projectile/area、无模板重定向与多预设验证（机制在，配置留宿主后续）。

## 粗估口径（假设）

六模块+Notation 骨壳最小集 + 本竖切 ≈ 2~3 周量级（单人、含验证；v1 估 2 周不含流程模板机制，v2 上调）。
