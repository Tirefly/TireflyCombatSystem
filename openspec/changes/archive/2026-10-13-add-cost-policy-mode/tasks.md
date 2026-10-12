## 1. Delta 编写

- [x] 1.1 `specs/skill-cost/spec.md`——3 条 MODIFIED（数据形状 / 运行态 / 契约键求值）；
      "自定义片段档具名降级"场景**删除**，同一需求下新增"空策略 fail-closed"场景
- [x] 1.2 `specs/skill-cooldown/spec.md`——2 条 MODIFIED（数据形状 / 终结侧时机分派）：
      仅 `ETcsCooldownTiming` → `ETcsSkillCooldownTiming` 改名（**值与语义逐字不动**）+ 对应场景名同步
- [x] 1.3 `specs/skill-def-asset/spec.md`——1 条 MODIFIED（字段形状）：`FTcsCostConfig` →
      `FTcsSkillCostConfig`；补"消耗字段的策略化"段；两处场景的判据同步

## 2. 校验

- [x] 2.1 `openspec validate add-cost-policy-mode --strict --no-interactive`
- [x] 2.2 归档后 `openspec validate --all --strict --no-interactive` = **42 passed / 0 failed**
- [x] 2.3 **MODIFIED 零丢失预检**：逐需求比对归档前后（标题一致 + 场景数不减）

## 3. 归档

- [x] 3.1 `openspec archive add-cost-policy-mode --yes`
- [x] 3.2 复核 `openspec/specs/` 内**旧类型名零命中**（`ETcsCostPolicy` / `FTcsCostConfig` /
      `ECP_*` / `ETcsCostTiming` / `ETcsCooldownTiming`）
      ——**注意**：归档目录 `changes/archive/` 内的历史提案**MUST NOT** 改写（那是现场记录），
      故校验只扫 `specs/` 与新建的 `changes/` 活跃区

> **★ 本提案的由来（元教训，MUST 读）**：代码在本轮**先**改并取证，而 `openspec/specs/`
> 仍描述已删除的类型 ⇒ **规格与代码分叉**。`openspec validate` **只校验格式与场景完整性，
> 不校验类型是否存在**，故旧类型名可静默留存（本批 grep 抓到 **41 处**）。
> **判据**：**每次删/改公开类型后，MUST grep 旧名全库（含 `openspec/specs/`）**，
> 不能只依赖 `validate` 的绿灯。
