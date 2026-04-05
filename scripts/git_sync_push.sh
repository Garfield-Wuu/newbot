#!/usr/bin/env bash
# Git 同步推送：在已 git add 的前提下提交，再 pull --rebase、push。
# 用法（在 newbot_ws 根目录或任意路径）:
#   cd ~/newbot_ws
#   git add <要提交的文件>
#   ./scripts/git_sync_push.sh "提交说明"
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

BRANCH="$(git rev-parse --abbrev-ref HEAD)"
REMOTE="${GIT_REMOTE:-origin}"

if [[ "${1:-}" == "" ]]; then
  echo "用法: $0 \"提交说明\"" >&2
  echo "请先 git add 要纳入本次提交的文件。" >&2
  exit 1
fi

if git diff --cached --quiet; then
  echo "暂存区为空，请先执行 git add ..." >&2
  exit 1
fi

git commit -m "$1"
git pull --rebase "${REMOTE}" "${BRANCH}"
git push "${REMOTE}" "${BRANCH}"
