## 1. 实现

- [x] 1.1 删旧三文件（`TcsTriggerRow.h` / `TcsTriggerConditions.h` / `.cpp`）
- [x] 1.2 新 `Public/Trigger/TcsEffectTrigger.h`——`FTcsEffectTriggerDef`（纯配置，含改名 `EffectChainId`、删 `Cues`）+ `ETcsExecutionGate`
- [x] 1.3 新 `Public/Trigger/TcsEffectTriggerInstance.h`——`FTcsEffectTriggerInstance` + `FTcsEffectTriggerHandle`
- [x] 1.4 新 `Public/Trigger/TcsTriggerCondition.h` + `Private/Trigger/TcsTriggerCondition.cpp`——条件注册表 + 自注册宏 + 两个内置条件 + 求值助手
- [x] 1.5 编译验证：UBT Development Editor **零警告零错误**（一次通过）
- [x] 1.6 旧类型名零残留自检（`grep FTcsTriggerRow` / `FTcsTriggerCondition_` 仅命中新文件）

## 2. 验证（禁 TDD 纪律：编译 + 定向人工检查）

- [x] 2.1 UBT Development Editor 编译零警告零错误
- [x] 2.2 依赖面自检：`grep "^#include"` 零领域模块（TcsEffect 只 include 自身 + TcsCore/TcsAttribute + 引擎）
- [ ] 2.3 **注册表运行期实证**——**未执行**：需 PIE 内登记一个条件后查表；本批零消费者（求值器/登记表属 Task 2），故留待 Task 2 的验收路径一并验。**成立依据是编译期自注册宏展开 + 注册表逻辑**，非实测
- [ ] 2.4 编辑器内 picker 检查（`Conditions` 数组的类型选择器）——**未执行**：需开编辑器

## 3. 规格

- [x] 3.1 `openspec validate refine-effect-trigger-shape --strict` 通过
- [x] 3.2 提案归档（`effect-trigger`：MODIFIED × 2 + ADDED × 1）
- [x] 3.3 规格库自检 `openspec validate --specs --strict` 全绿

## 4. 未覆盖（如实记）

- **无运行期验收**：本批仍只落数据形状与注册表**机制**，**无调用方**（登记表/求值器属 Task 2）。故 2.1/2.2 是静态检查，2.3/2.4 需编辑器或 PIE——**本批没有任何行为实证**。
- **反射注册入口未做**：`Register` 是纯 C++ 面（`TFunction` 不可反射）。这是与步骤执行器注册表**共享的欠账**（CS 调研 §7.6 G-2），须**同批**解决——不在此处单独开一个反射入口（否则两处口径不一）。
- **上下文扩展机制未做**：`FTcsTriggerContext` 的 PV-1 式结构体继承今天零消费者；Buff 生命周期参数（`StateHandle`/`Stacks`/`Level`）要等 M3 落地才有真实数据源。
- **资产载体未做**：`UTcsEffectTriggerDef` + DefLibrary 发现路径是独立一块（单列 Task）。
