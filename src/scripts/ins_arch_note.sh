#!/usr/bin/env bash
set -euo pipefail
F=/home/hllrgb/sh-dev/src/ARCH.md
L=$(grep -n '^  WE_TILDE' "$F" | head -1 | cut -d: -f1)
# Insert note after WE_TILDE + empty + ``` (3 lines after WE_TILDE)
TARGET=$((L + 3))
# replace_lines on the blank line after ```
/home/hllrgb/sh-dev/src/utils/replace_lines "$F" $TARGET $TARGET <<'EOF'

注：最终设计无 WE_BRACE 和 WE_QUOTE。
Brace exp 在 Phase 1（文本预扫）完成——分裂 raw WORD_T（保留引号），不生成 WE 类型。
引号处理在 Phase 2 parse_word 内逐层独立 bool in_quote，不单独成码。
EOF
echo "=== Note inserted ==="
