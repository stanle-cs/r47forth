#!/usr/bin/env bash
# Out-of-family dispatch for the cross-model audit.
#
# Encodes the reader-pool traps that cost twelve runs on 2026-08-06:
#   - agy: --model must come BEFORE -p, or Claude silently answers. Print
#     mode has a 5-minute default timeout and returns EMPTY on exceed.
#   - codex/Sol: completes ONLY self-contained packets — empty cwd,
#     read-only sandbox (no shell needed, so the missing-bubblewrap wall
#     never bites), reasoning medium, hard `timeout` backstop. Anything
#     that lets it explore reads exhaustively and never concludes.
#   - Both: the reply must open with `MODEL: <name>` (the packet template
#     asks for it) and the name is verified here. Empty output is a
#     timeout or a CLI failure, never a clean bill.
#
# Usage:
#   dispatch.sh probe  [gemini|sol|all]
#   dispatch.sh gemini <packet.md> [reply-out.md]
#   dispatch.sh sol    <packet.md> [reply-out.md]
#
# Env overrides: GEMINI_MODEL (gemini-3.1-pro-high), GEMINI_TIMEOUT (12m),
# SOL_MODEL (gpt-5.6-sol), SOL_EFFORT (medium), SOL_TIMEOUT (900 s),
# CROSSAUDIT_SKIP_LINT=1 to dispatch a packet the linter rejects,
# CROSSAUDIT_FORCE=1 to overwrite an existing non-empty reply file.
set -euo pipefail

GEMINI_MODEL="${GEMINI_MODEL:-gemini-3.1-pro-high}"
GEMINI_TIMEOUT="${GEMINI_TIMEOUT:-12m}"
SOL_MODEL="${SOL_MODEL:-gpt-5.6-sol}"
SOL_EFFORT="${SOL_EFFORT:-medium}"
SOL_TIMEOUT="${SOL_TIMEOUT:-900}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

die() { echo "dispatch: $*" >&2; exit 2; }

lint() {
  [ "${CROSSAUDIT_SKIP_LINT:-0}" = 1 ] && return 0
  python3 "$HERE/packet_lint.py" "$1" || die \
    "packet failed HARD lint checks — the packet is the audit; fix it first (CROSSAUDIT_SKIP_LINT=1 to override)"
}

# A second dispatch over the same packet defaults to the SAME reply path and
# silently destroys the first reply — the only Gemini Pro reply over
# derivchain.md died this way (2026-08-29). Replies are audit evidence: the
# report and the workflow's outOfFamily accounting both cite these paths.
noclobber() {
  [ "${CROSSAUDIT_FORCE:-0}" = 1 ] && return 0
  [ -s "$1" ] && die \
    "reply file $1 already exists and is non-empty — refusing to overwrite audit evidence. Pass an explicit reply-out path for a second reader, or CROSSAUDIT_FORCE=1."
  return 0
}

# check_identity <reply-file> <must-match-regex> <reader-name>
check_identity() {
  local f="$1" want="$2" who="$3" head model
  [ -s "$f" ] || die \
    "$who returned EMPTY output — that is a timeout or a CLI failure, NOT a clean audit. For agy, raise --print-timeout; for codex, raise SOL_TIMEOUT."
  head="$(head -n 10 "$f")"
  model="$(grep -m1 -oiE 'MODEL:.*' "$f" || true)"
  if echo "$head" | grep -qiE "$want"; then
    echo "dispatch: identity OK — ${model:-$who matched /$want/}"
  else
    die "$who identity check FAILED (reply opens: '$(echo "$head" | head -n 2 | tr '\n' ' ')'). \
A silent fallback is indistinguishable from a good audit; if a Claude answered, the family exclusion failed open and these findings must be DISCARDED. \
For agy: --model must come BEFORE -p, and the model must be on \`agy models\`."
  fi
  if [ "$who" != "claude-check" ] && echo "$head" | grep -qi claude; then
    die "$who reply names Claude — same-family reader, findings must be DISCARDED."
  fi
}

run_gemini() {
  local pkt="$1" out="${2:-${1%.md}.gemini.reply.md}"
  noclobber "$out"
  lint "$pkt"
  echo "dispatch: gemini ($GEMINI_MODEL, --print-timeout $GEMINI_TIMEOUT) <- $pkt"
  # Flag order is load-bearing: --model BEFORE -p, prompt as argument not stdin.
  agy --model "$GEMINI_MODEL" --print-timeout "$GEMINI_TIMEOUT" \
      -p "$(cat "$pkt")" > "$out" 2> "$out.err" || true
  check_identity "$out" 'gemini' "gemini"
  echo "$out"
}

run_sol() {
  local pkt out wd
  pkt="$(readlink -f "$1")"
  out="$(readlink -f "$(dirname "${2:-$1}")")/$(basename "${2:-${1%.md}.sol.reply.md}")"
  noclobber "$out"
  lint "$pkt"
  wd="$(mktemp -d)"   # EMPTY dir: give Sol nothing to explore.
  echo "dispatch: sol ($SOL_MODEL, effort $SOL_EFFORT, timeout ${SOL_TIMEOUT}s, cwd $wd) <- $pkt"
  ( cd "$wd" && timeout "$SOL_TIMEOUT" \
      codex exec -s read-only --skip-git-repo-check \
        -m "$SOL_MODEL" -c model_reasoning_effort="$SOL_EFFORT" \
        -o "$wd/out.txt" - < "$pkt" > "$wd/run.log" 2>&1 ) || true
  [ -f "$wd/out.txt" ] && cp "$wd/out.txt" "$out" || : > "$out"
  cp "$wd/run.log" "$out.err" 2>/dev/null || true
  rm -rf "$wd"
  check_identity "$out" 'gpt|codex|sol' "sol"
  echo "$out"
}

probe() {
  local which="${1:-all}" p
  p="$(mktemp --suffix=.md)"
  printf 'Begin your reply with the line `MODEL: <your exact model name>`.\nThen reply with exactly one more line: PROBE-OK. Run no commands.\n' > "$p"
  export CROSSAUDIT_SKIP_LINT=1
  export CROSSAUDIT_FORCE=1   # probes reuse fixed /tmp paths on purpose
  case "$which" in
    gemini) GEMINI_TIMEOUT=4m run_gemini "$p" /tmp/crossaudit-probe-gemini.md ;;
    sol)    SOL_TIMEOUT=240 run_sol "$p" /tmp/crossaudit-probe-sol.md ;;
    all)    GEMINI_TIMEOUT=4m run_gemini "$p" /tmp/crossaudit-probe-gemini.md
            SOL_TIMEOUT=240 run_sol "$p" /tmp/crossaudit-probe-sol.md ;;
    *) die "probe target must be gemini, sol, or all" ;;
  esac
  rm -f "$p"
}

case "${1:-}" in
  probe)  shift; probe "$@" ;;
  gemini) shift; run_gemini "$@" ;;
  sol)    shift; run_sol "$@" ;;
  *) die "usage: dispatch.sh probe|gemini|sol ..." ;;
esac
