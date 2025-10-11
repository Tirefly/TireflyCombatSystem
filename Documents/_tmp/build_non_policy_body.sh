set -euo pipefail
work=Plugins/TireflyCombatSystem/Docs/_tmp
body="/non_policy_body.md"
:> ""
# 1) 整体方案（去“附：策略调用时机”小节）
awk 'NR<56 || NR>60{print}' Plugins/TireflyCombatSystem/Docs/TCS_状态系统整体方案_顶层StateTree_槽位_实例联动与扩展.md >> ""
echo >> ""; echo '---' >> ""; echo >> ""
# 2) 顶层映射联动实现（去“策略调用时机（设计）”3行）
awk 'NR!=40 && NR!=41 && NR!=42{print}' Plugins/TireflyCombatSystem/Docs/TCS_顶层StateTree与槽位映射联动实现方案.md >> ""
echo >> ""; echo '---' >> ""; echo >> ""
# 3) 调用顺序图：仅保留 2) 3) 5) 6)
sed -n '/^## 2\)/,/^## 3\)/p' Plugins/TireflyCombatSystem/Docs/TCS_调用顺序图_关键模块.md >> ""
sed -n '/^## 3\)/,/^## 4\)/p' Plugins/TireflyCombatSystem/Docs/TCS_调用顺序图_关键模块.md >> ""
sed -n '/^## 5\)/,/^## 6\)/p' Plugins/TireflyCombatSystem/Docs/TCS_调用顺序图_关键模块.md >> ""
sed -n '/^## 6\)/,/^## 7\)/p' Plugins/TireflyCombatSystem/Docs/TCS_调用顺序图_关键模块.md >> ""
echo >> ""; echo '---' >> ""; echo >> ""
# 4) 示例（去策略/免疫/净化片段）
# 英雄联盟
awk '1' Plugins/TireflyCombatSystem/Docs/示例：TCS_英雄联盟_严格对齐数据结构.md  | sed '/^## 5\. 策略资产（Policies）/,'  | sed '/规则层对.*返回免疫/d'  | sed '/Cleanse 任务参数/{N;N;N;N;N;N;N;d}'  | sed '/轻量前置（可选，StateManager.CheckImmunity）/d' >> ""
echo >> ""; echo '---' >> ""; echo >> ""
# 卧龙
awk '1' Plugins/TireflyCombatSystem/Docs/示例：TCS_卧龙苍天陨落_严格对齐数据结构.md  | sed '/^## 5\. 策略资产（Policies）/,'  | sed '/- Cleanse（示例，YAML，仅示意）/,+15d'  | sed '/轻量前置（可选，StateManager.CheckImmunity）/d' >> ""
echo >> ""; echo '---' >> ""; echo >> ""
# 鬼泣5
awk '1' Plugins/TireflyCombatSystem/Docs/示例：TCS_鬼泣5_严格对齐数据结构.md  | sed '/^## 5\. 策略资产（Policies）/,'  | sed '/轻量前置（可选，StateManager.CheckImmunity）/d' >> ""

# Build final doc with title, intro, ToC, then body
final=Plugins/TireflyCombatSystem/Docs/TCS_阶段划分_即将开发_非策略部分.md
{
  echo '# TCS 阶段划分：即将开发的实现文档（非策略部分）'; echo;
  echo '说明';
  echo '- 本文聚合并保留与顶层 StateTree、槽位、实例、合并器、激活模式、计时/抢占/排队/调试等相关的全部内容；';
  echo '- 策略解析器/免疫/净化相关内容已迁移至：TCS_阶段划分_暂缓设计_策略解析_免疫_净化.md';
  echo; echo '---'; echo;
  echo '## 目录'; echo;
  grep -E '^#{1,3} ' "" | sed -E 's/^### (.*)$/    - \1/; s/^## (.*)$/  - \1/; s/^# (.*)$/- \1/';
  echo; echo '---'; echo;
  cat "";
} > ""
