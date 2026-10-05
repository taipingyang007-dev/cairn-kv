#!/usr/bin/env bash
# cairn 端到端验收：重构的安全网。每搬完一个类就跑一次。
# 用法：  bash tests/acceptance.sh            （自动起停服务）
#         bash tests/acceptance.sh --keep     （用已在跑的服务，不自己起）
set -u
PORT=6380
BIN=./build/bin/cairn
PASS=0; FAIL=0; FAILED_NAMES=()

R() { timeout 2 redis-cli -p $PORT "$@" 2>&1; }

check() {  # check <名字> <期望> <实际>
    if [ "$2" == "$3" ]; then
        PASS=$((PASS+1))
    else
        FAIL=$((FAIL+1)); FAILED_NAMES+=("$1")
        printf "  \033[31mFAIL\033[0m  %-28s 期望[%s]  实际[%s]\n" "$1" "$2" "$3"
    fi
}

# ---------- 先编译（否则会拿旧二进制跑出假绿）----------
if [ "${SKIP_BUILD:-0}" != "1" ]; then
    if ! OUT=$(cmake --build build 2>&1); then
        echo "$OUT" | grep -E "error|Error" | head -10
        printf "  \033[31m编译失败\033[0m —— 验收不跑。（旧二进制跑出来的绿是假的）\n" >&2
        exit 3
    fi
    if echo "$OUT" | grep -q "warning:"; then
        echo "$OUT" | grep "warning:" | head -5
        printf "  \033[33m有警告\033[0m（不阻断，但要清掉）\n" >&2
    fi
fi

# ---------- 起服务 ----------
OWN_SERVER=0
if [ "${1:-}" != "--keep" ]; then
    if ss -ltn 2>/dev/null | grep -q ":$PORT "; then
        echo "端口 $PORT 已被占用，请先停掉，或用 --keep 复用它" >&2; exit 2
    fi
    [ -x "$BIN" ] || { echo "找不到 $BIN，先 cmake --build build" >&2; exit 2; }
    $BIN & SRV=$!; OWN_SERVER=1
    for _ in $(seq 20); do ss -ltn 2>/dev/null | grep -q ":$PORT " && break; sleep 0.1; done
fi
cleanup() { [ $OWN_SERVER -eq 1 ] && kill $SRV 2>/dev/null; }
trap cleanup EXIT

# ---------- A. D5 十二条 ----------
R FLUSH_NOT_A_COMMAND >/dev/null            # 预热连接
check "PING"                 "PONG"        "$(R PING)"
check "SET name jixing"      "OK"          "$(R SET name jixing)"
check "GET name"             "jixing"      "$(R GET name)"
check "SET name taipingyang" "OK"          "$(R SET name taipingyang)"
check "GET name (覆盖后)"     "taipingyang" "$(R GET name)"
check "GET nokey → nil"      ""            "$(R GET nokey)"
check "SET empty ''"         "OK"          "$(R SET empty '')"
check "GET empty → 空串"      ""            "$(R GET empty)"
check "DEL name → 1"         "1"           "$(R DEL name)"
check "DEL name → 0"         "0"           "$(R DEL name)"
check "GET name (删后)"       ""            "$(R GET name)"
check "SET onlyonearg"       "ERR wrong number of arguments for 'SET' command" "$(R SET onlyonearg)"

# ---------- B. 边界与 arity ----------
check "SET k (缺 value)"     "ERR wrong number of arguments for 'SET' command" "$(R SET k)"
check "SET k v extra (多给)"  "OK"          "$(R SET k v extra)"
check "GET (缺 key)"          "ERR wrong number of arguments for 'GET' command" "$(R GET)"
check "GET a b (多给)"        "ERR wrong number of arguments for 'GET' command" "$(R GET a b)"
check "DEL (缺 key)"          "ERR wrong number of arguments for 'DEL' command" "$(R DEL)"
check "ECHO hi"              "hi"          "$(R ECHO hi)"
check "ECHO (缺参数)"         "ERR wrong number of arguments for 'ECHO' command" "$(R ECHO)"
check "get nokey (小写)"      ""            "$(R get nokey)"
check "PiNg (混合大小写)"      "PONG"        "$(R PiNg)"
check "FOOBAR (未知命令)"      "ERR unknown command 'FOOBAR'" "$(R FOOBAR)"
check "COMMAND → 空数组"      ""            "$(R COMMAND)"

# ---------- C. 字节流层（流水线 / 半包 / 无重复）----------
STREAM=$(python3 - "$PORT" <<'PY'
import socket, sys, time
port = int(sys.argv[1]); out = []
def drain(s):
    s.setblocking(False)
    try: extra = s.recv(65536)
    except BlockingIOError: extra = b""
    s.setblocking(True); return extra
try:
    s = socket.create_connection(("127.0.0.1", port), timeout=3)
    s.sendall(b"*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\nbar\r\n"); time.sleep(0.1); s.recv(4096)
    GET, REPLY = b"*2\r\n$3\r\nGET\r\n$3\r\nfoo\r\n", b"$3\r\nbar\r\n"

    # 流水线 16 条 → 正好 16 条回复
    s.sendall(GET*16); time.sleep(0.25)
    d = b""
    while len(d) < len(REPLY)*16:
        c = s.recv(65536)
        if not c: break
        d += c
    out.append(("流水线16条", "ok" if d == REPLY*16 else f"收到{len(d)}字节"))
    out.append(("流水线无残留", "ok" if drain(s) == b"" else "有多余字节"))

    # 连续三轮 → output 不累积
    acc = "ok"
    for _ in range(3):
        s.sendall(GET*4); time.sleep(0.15)
        d = b""
        while len(d) < len(REPLY)*4:
            c = s.recv(65536)
            if not c: break
            d += c
        if d != REPLY*4 or drain(s) != b"": acc = "有累积/残留"
    out.append(("三轮无累积", acc))

    # 半包：拆两半
    s.sendall(b"*2\r\n$3\r\nGET\r\n$3\r\nf"); time.sleep(0.15)
    out.append(("半包不提前回复", "ok" if drain(s) == b"" else "提前回了"))
    s.sendall(b"oo\r\n"); time.sleep(0.15)
    out.append(("补齐后正确回复", "ok" if s.recv(4096) == REPLY else "回复不对"))
    s.close()
except Exception as e:
    out.append(("字节流测试", f"异常 {e}"))
print("\n".join(f"{a}\t{b}" for a, b in out))
PY
)
while IFS=$'\t' read -r name got; do
    [ -z "$name" ] && continue
    check "$name" "ok" "$got"
done <<< "$STREAM"

# ---------- D. 服务器还活着 ----------
check "跑完仍然存活"          "PONG"        "$(R PING)"

# ---------- 汇总 ----------
TOTAL=$((PASS+FAIL))
echo "─────────────────────────────────────────"
if [ $FAIL -eq 0 ]; then
    printf "  \033[32m全部通过\033[0m  %d/%d\n" "$PASS" "$TOTAL"; exit 0
else
    printf "  \033[31m失败 %d 项\033[0m  （通过 %d/%d）：%s\n" "$FAIL" "$PASS" "$TOTAL" "${FAILED_NAMES[*]}"; exit 1
fi
