# value-convention Specification

## Purpose
TBD - created by archiving change add-tcsnotation-value-convention. Update Purpose after archive.
## Requirements
### Requirement: 值约定标志位

TcsNotation MUST 提供 `ETcsValueConventionFlag`（UENUM，Bitflags + UseEnumValuesAsMaskValuesInEditor）：`VCF_None = 0`（默认/恒等）、`VCF_Percent = 1 << 0`（策划写 85 → 规范 0.85）、`VCF_OneMinus = 1 << 1`（取 1−v，与 Percent 组合：写 25"减少25%" → 0.75）、`VCF_Negate = 1 << 2`（取负：写 1000"减少1000生命" → -1000）。

#### Scenario: 标志位可按位组合配置

- **WHEN** 在编辑器检查约定列属性
- **THEN** 以位掩码形式多选（None 恒等不参与组合）

### Requirement: 规范值转换助手

TcsNotation MUST 提供静态无状态助手 `FTcsValueConvention::ConvertToCanonical(double RawValue, int32 ConventionFlags)`（header-only）：按**固定组合顺序 Percent（÷100）→ OneMinus（1−v）→ Negate（取负）**应用各标志位（D5-18 拍板——顺序即语义，不因标志书写顺序改变）；无任何标志时恒等返回。转换只发生在**写入点**（物化/注册边界）——运行侧账本/快照恒为规范值，机制层（TcsEffect/TcsDamage/TcsTargeting）不加 Notation 边。

#### Scenario: 三变换固定顺序组合

- **WHEN** 调用 `ConvertToCanonical(85, VCF_Percent | VCF_OneMinus | VCF_Negate)`
- **THEN** 返回 -0.15（0.85 → 0.15 → -0.15）

#### Scenario: 无约定恒等

- **WHEN** 调用 `ConvertToCanonical(x, VCF_None)`
- **THEN** 返回 x（恒等）

