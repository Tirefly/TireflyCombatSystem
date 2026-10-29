## 1. 规格 delta（已完成）

- [x] 1.1 `primary-asset-loading` MODIFIED「运行期加载三件与防重」：策略升为契约 + 三档兑现判据 + `OnDemand` 目录形态 + 查重拒绝登记 + 装配型禁令
- [x] 1.2 `primary-asset-loading` ADDED「按 Def 类型的加载策略配置」（`FPrimaryAssetType` 键 / `FTcsAssetIdList` 清单壳）
- [x] 1.3 `primary-asset-loading` ADDED「按策略异步预载并驱动就绪与失败」（发现/加载分离 + handle 持有 + `OnDefinitionsFailed`）
- [x] 1.4 `primary-asset-loading` ADDED「定义解析口支持异步」（`ETcsResolveMode` + `FStreamableDelegate`，默认 `Async`，5 个口去 `const`）
- [x] 1.5 `integration-entity` MODIFIED「定义库（GameInstance 级最小版）」（含弱引用待装配集合 + 失败阻断 + 解析口异步）
- [x] 1.6 `openspec validate --strict` 通过

## 2. 设置面

- [x] 2.1 `UTcsAssetButlerSettings` 加 `LoadPolicyByType`（`TMap<FPrimaryAssetType, ETcsAssetLoadPolicy>`）
- [x] 2.2 `UTcsAssetButlerSettings` 加 `SelectedAssetsByType`（`TMap<FPrimaryAssetType, FTcsAssetIdList>`）——**值 MUST 包一层 USTRUCT**，UHT 硬拒容器作 TMap 值
- [x] 2.3 生效策略解析 `ResolveLoadPolicy`：配置优先 → 回落宏声明；`PreloadSelected` 缺清单留具名条目

## 3. 定义库：发现与加载分离

- [x] 3.1 六条 `Discover*` 收口为一条 `DiscoverDefs`（只取 `FAssetData`，**零加载**）
- [x] 3.2 `OnDemand` 目录容器：`TMap<FPrimaryAssetType, TArray<FPrimaryAssetId>>`（**不建 `DefTag → Id` 映射**）+ `GetOnDemandCatalog` / `GetOnDemandCatalogCount`
- [x] 3.3 按生效策略分流加载（全部异步）；`PreloadAll` 走 `CheckDuplicates` 拒绝登记
- [x] 3.4 `FStreamableHandle` 由 `PreloadHandles` 持有至 `Deinitialize`
- [x] 3.5 就绪置位与 `OnDefinitionsReady` 移到 `OnPreloadFinished`；失败则 `OnDefinitionsFailed` + 永不置位 + 一次性 Error 全清单
- [x] 3.6 待装配世界集合（`TArray<TWeakObjectPtr<UWorld>> PendingWorlds`）：未就绪时登记、就绪回调里装配并清空、失效跳过；`SeedWorld` 内部另有一道就绪守卫

## 4. 解析口改签名

- [x] 4.1 定义 `ETcsResolveMode{Async, Sync}`（`TcsResolveMode.h`）
- [x] 4.2 5 个 `Resolve*` 加参数（`Mode = Async` + `FStreamableDelegate OnLoaded`，**按值传**），全部去 `const`；`Async` 返 `nullptr`、已在内存则同步触发代理
- [x] 4.3 装置 `TcsDev` 的 10 处调用显式传 `Sync`（保持原行为）

## 5. 自注册与 abstract

- [x] 5.1 `UTcsStateDef` 加 `abstract`（**不加注册**）
- [x] 5.2 `UTcsSkillDef` 加 `UE_DEFINE_PRIMARY_ASSET_TYPE` + `TcsSkill.Build.cs` 加 `TcsAssetButler`
- [x] 5.3 修正器模板 / 技能两条发现也迁到 `FTcsPrimaryAssetQuery::GetAssetsForDefClass`（六族统一）
- [x] 5.4 4 个装配型宏声明由 `OnDemand` 改为 `PreloadAll`（链 / 触发 / 状态 / 属性），技能注册即 `PreloadAll`；`UTcsAttrModDef` 保持 `OnDemand`（唯一合法用例）

## 6. 规格回写（本体落盘）

- [x] 6.1 `openspec/specs/primary-asset-loading/spec.md` 本体应用 delta（10 需求 / 30 场景）
- [x] 6.2 `openspec/specs/integration-entity/spec.md` 本体应用 delta（3 需求 / 43 场景）
- [x] 6.3 `openspec validate --all --strict` 全绿（49 passed / 0 failed）
- [x] 6.4 **逐条比对本体与代码**（`INDEX` §8 的 R-19 判据）——14 项对照全命中

## 7. 档案回写

- [ ] 7.1 `Documents/tcs-arch/03-asset-butler.md`：D-4 / U-1 / U-5 标闭合
- [ ] 7.2 `Documents/tcs-arch/INDEX.md`：R-21 两处

## 8. 验证

- [x] 8.1 双配置编译 0 error 0 warning（`LegendAutoChessEditor Win64 Development` + `LegendAutoChess Win64 Shipping`）
- [ ] 8.2 **PIE**：三档读数（`PreloadAll` 全量 / `OnDemand` 零加载 / `PreloadSelected` 只装清单）
- [ ] 8.3 **PIE**：改一个类型的策略值 → 该类型已加载资产数随之改变（策略非假的判据）
- [ ] 8.4 **PIE**：`OnDemand` 首次 `Sync` 解析成功；`Async` 解析返空且代理触发
- [ ] 8.5 **PIE**：加载失败 → `IsRuntimeReady` 恒 false + `OnDefinitionsFailed` 广播 + 待装配世界保持等待
- [x] 8.6 反查：`GetAssetsByClass` 在 `TcsDefinitionSubsystem*` **零命中**（仅注释提及）
- [x] 8.7 反查：`AssetData.GetAsset()` 在发现路径 **零命中**（仅注释提及）

---

## 实施期新增裁定（2026-10-26，用户逐条确认）

| # | 问题 | 裁定 |
|---|---|---|
| **A-1** | `OnDemand` 与「装配到世界」互斥（5 个装配型类型今天全是 `OnDemand`，忠实落地 = 装配为空 = 全站失效） | **默认值走「宏就是真相」**：4 个装配型宏改 `PreloadAll`（技能注册即 `PreloadAll`）；`UTcsAttrModDef` 保持 `OnDemand`（唯一合法用例）。**不设「全局默认策略」字段**——那会让宏里的值变成看得见、不起作用的假真相 |
| **A-2** | 装配型类型被宿主配成 `OnDemand` 时怎么反应 | **硬拒**：计入失败清单 + Error + 定义库不就绪；MUST NOT「警告 + 自动改回 `PreloadAll`」（那是静默改写配置值，与本变更要消灭的假策略同罪） |
| **A-3** | `TMap<FPrimaryAssetType, TArray<FPrimaryAssetId>>` 被 UHT 硬拒（容器不能作 TMap 值） | 值包一层 `USTRUCT` `FTcsAssetIdList`；语义不变，MUST NOT 改扁平数组 |
| **A-4** | `FStreamableDelegate& InOutOnLoaded = 默认空代理` 不是合法 C++ | 改为**按值传** `FStreamableDelegate OnLoaded = FStreamableDelegate()` |
| **A-5** | 规格里列的 `ResolveAttributeDef` **全仓不存在**（`ARCH-04` U-1 明文不给 GI 侧读口） | 解析口是 **5 个不是 6 个**，规格已订正 |

## 剩余待定

| # | 问题 | 结论 |
|---|---|---|
| Q-1 | `Sync` 模式与「插件默认不同步加载」的措辞关系 | 已定：规格写「插件默认路径必须异步；同步只作解析口逃生口」 |
| Q-2 | `ResolveChain`（`ChainId`）/ `ResolveAttrModDef`（`TemplateTag`）的键名是否统一 | 保持现状——键名归各域身份定义，统一命名会制造与资产身份不符的假抽象 |

---

## 与批次 A 的关系（用户裁定：**先 B 后 A**）

- B 落地后「未就绪」成为**常态** ⇒ 批次 A（`fix-entity-component-late-registration`）才真正被行使；
- **A MUST 追加一个分支**：`OnDefinitionsFailed` 下组件 MUST 放弃并留 Error——否则「失败即永不就绪」会让组件永久静默挂起。
