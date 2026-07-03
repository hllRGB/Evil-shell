#!/usr/bin/env bash
set -euo pipefail
F=/home/hllrgb/sh-dev/src/ARCH.md

# Insert lexer design section after "WE_LITERAL("bcd"): 追加到末串 → `2bcd`" line
L=$(grep -n 'WE_LITERAL("bcd")' "$F" | head -1 | cut -d: -f1)
INSERT=$((L + 1))

/home/hllrgb/sh-dev/src/utils/replace_lines "$F" "$INSERT" "$INSERT" <<'LEXDESIGN'

### 主 parser / lexer 架构

#### Lexer 职责边界

lexer 只识别 **结构性分隔符**。所有 word 内容统一交 parse_word：

| 类别 | 字符 | lexer 处理 | parse_word 处理 |
|------|------|-----------|-----------------|
| 空白 | `' ' \t` | 跳过 | 不涉及 |
| 结构化 | `\n` `;` `&` `\|` `(` `)` | 返回对应 TOK_ | 不涉及 |
| 重定向 | `>` `<` 及其组合(>> << >& 等) | 返回 TOK_REDIR | 不涉及 |
| 数字+重定向 | `2>` `3>>` 等 | 返回 TOK_REDIR + fd | 不涉及 |
| 转义 | `\` | 不处理 | 入 parse_word 处理 |
| 展开 | `$` `` ` `` | 不处理 | 入 parse_word 处理(WE_VAR/CMDSUBST/ARITH) |
| 引号 | `'` `"` | 不处理 | 入 parse_word 处理(in_quote 追踪) |

#### Token 类型

```
关键字: TOK_IF THEN ELSE ELIF FI CASE ESAC FOR SELECT
         WHILE UNTIL DO DONE FUNCTION COPROC IN BANG TIME
逻辑:   TOK_AND_AND(&&) OR_OR(||) PIPE(|) BAR_AND(|&)
列表:   TOK_SEMI(;) AMP(&) NEWLINE EOF
Case:   TOK_DSEMI(;;) SEMI_AMP(;&) DSEMI_AMP(;;&)
特殊:   TOK_COND_START([[) COND_END(]]) ARITH_CMD((()))
分组:   TOK_LPAREN(() RPAREN())
值:     TOK_WORD TOK_ASSIGNMENT_WORD
```

每个 TOKEN_T 携带 `start` / `end` 指针标记在输入流中的位置（用于错误报告和高亮）。

#### checkkwd 机制（dash 风格）

lexer 不自知关键字。由 parser 通过 flag 控制：

```
CHKNL   — lex 时吞掉后续 \n（列表上下文）
CHKKWD  — 读到原始字符后优先匹配关键字表
CHKALIAS— alias 展开（预留）
```

流程：lexer 读到一段原始字符 → 若 CHKKWD 设置 → 比对关键字表 → 匹配则返回 TOK_IF 等 → 全部失败才返回 TOK_WORD。

关键字提升条件：原始字符为纯字面且无引号包围（`"if"` 不会误匹配为 TOK_IF）。
含引号的词无条件走 TOK_WORD（parser 不直接认知引号，引号是 parse_word 的职责）。

#### parser 语法结构（递归下降）

```
inputunit → parse_inputunit()
  compound_list → parse_compound_list()
    list0/list1 → parse_list()
      pipeline_command → parse_pipeline_command()
        pipeline → parse_pipeline()
          command → parse_command()
            simple_command → parse_simple_command()
            shell_command → parse_shell_command()
              for/while/until → parse_for() / parse_while() / parse_until()
              if/case/select  → parse_if() / parse_case() / parse_select()
              subshell/group  → parse_subshell() / parse_group()
              function_def    → parse_function_def()
            simple_command_element → parse_simple_command_element()
            redirection → parse_redirection()
```

每个 parse 函数调用 `lex()` 时传入所需 context flags（checkkwd），不自行处理底层字符流。

LEXDESIGN

echo "=== Lexer architecture section inserted ==="
