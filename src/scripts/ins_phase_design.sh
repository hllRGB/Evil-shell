#!/usr/bin/env bash
set -euo pipefail
F=/home/hllrgb/sh-dev/src/ARCH.md

# Insert Phase 1/Phase 2 design after "operate.c 遍历 WE_T[] 逐节点求值。" line
L=$(grep -n 'operate.c 遍历 WE_T\[] 逐节点求值。' "$F" | head -1 | cut -d: -f1)
INSERT=$((L + 1))

/home/hllrgb/sh-dev/src/utils/replace_lines "$F" "$INSERT" "$INSERT" <<'WORDDESIGN'

### Word parser 两阶段

#### Phase 1: Brace pre-scan（纯文本层，保留引号）
扫描 `{...,...}` 或 `{x..y[..z]}` 序列表达式，在原 WORD_T 字符流上分裂。跳过抑制区域：

| 构造 | 跳过范围 |
|------|----------|
| `${` | 到匹配 `}`（`{1,2}` 中 `}` 双重角色：闭 brace 也闭 param exp） |
| `$(` | 到匹配 `)` |
| `$((` | 到匹配 `))`（追踪 `()` 深度，C 子表达式不干扰） |
| `<()` `>()` | 到匹配 `)` |

**扫描算法：** 从左找首个 `}`，回溯最近 `{`，检查其间有无逗号或 `..`。无效则继续。
有效 brace → 分裂为 WORD_LIST_T（多个 raw WORD_T，引号完整保留）。无 brace → 直进 Phase 2。

#### Phase 2: WE_T 编码
单 WORD_T 字符流 → `WE_T[]`。共享 INPUT 游标，每层独立 `in_quote`。

字符 switch：
- `\c` → 转义，`'...'` → 单引号字面
- `"` → 切换本层 in_quote（不影响父层）
- `$` + `(` → WE_CMDSUBST / WE_ARITH
- `$` + `{` → WE_PARAM_AND_VAR：读 name + op，扫描首个非引号内 `}` 定界 word，递归 parse_word
- `$` + id → WE_PARAM_AND_VAR simple
- `~` 词首 → WE_TILDE
- 其他 → WE_LITERAL 累计

#### 多串展开（`$@` / `${array[@]}` / `$*` / `${array[*]}`）
运行时单 WORD_T 可产出 N 个字符串。引号影响展开行为：

| 形式 | 展开行为 | 词数 |
|------|----------|------|
| 无引号 `$@` / `${arr[@]}` | 每参数独立成词 → word splitting + glob | 增 |
| `"$@"` / `"${arr[@]}"` | 每参数独立成词，跳过 splitting + glob | 增 |
| 无引号 `$*` / `${arr[*]}` | IFS 首字 join → word splitting + glob | 同或增 |
| `"$*"` / `"${arr[*]}"` | IFS 首字 join，单字，无 splitting/glob | 1 |

操作模型（operate.c 维护动态字符串数组）：
```
正常 WE_T          → 追加到当前串
$@ / ${arr[@]}     → 首 el 追加当前串，后续 el 各开新串
$* / ${arr[*]}     → IFS 首字 join，追加当前串（始终单串）
空 $@（0 el）       → 产出 0 串，word 消除
```

示例：`"${array[@]}bcd"`（array=(1 2)）→ argv: [`1`, `2bcd`]
- WE_PARAM_AND_VAR: 首 el `1` 追加当前串，开新串，`2` 入新串
- WE_LITERAL("bcd"): 追加到末串 → `2bcd`

WORDDESIGN

echo "=== Phase 1/Phase 2 design inserted ==="
