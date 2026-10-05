# cairn 笔记 · 工具与踩坑

> 命令速查 + 环境坑。**这一份不需要复述**，是查阅用的。

---

## 附：git 输出速查卡（工具卡，非知识点，不用复述）

> 先敲一次 `git config --global core.quotepath false`，否则中文文件名显示成 `\350\277\233...` 转义乱码。

| 英文 | 意思 |
|---|---|
| `On branch main` | 你站在 main 分支上 |
| `Changes to be committed` | **已进篮子**（暂存区），提交时会带上 → 显示绿色 |
| `Changes not staged for commit` | 改了但**还没进篮子** → 显示红色 |
| `Untracked files` | git 还不认识的新文件 |
| `modified:` / `deleted:` / `new file:` | 改过 / 删了 / 新增 |
| `(use "git add <file>..." ...)` | 圆括号行全是操作提示，可跳过 |
| `nothing to commit, working tree clean` | 干干净净，没有可提交的东西 |
| `no changes added to commit` | 篮子是空的，先去 git add |
| diff 里行首 `-`（红）/ `+`（绿） | 删掉的行 / 新加的行（按 q 退出） |
| `@@ -10,5 +10,6 @@` | 行号坐标，初期不用细看 |
| `[main ab12cd3] <消息>` | 提交成功：落在 main，短编号 ab12cd3 |
| `4 files changed, 120 insertions(+), 30 deletions(-)` | 动了 4 个文件，增 120 行、删 30 行 |
| `HEAD -> main` | HEAD = 你现在站的位置 |
| `Co-Authored-By: ...` | 提交信息里的合作者署名行 |
| `fatal:` / `error:` | 出错了——整段贴给 Claude |

**按场景记的五条（2026-09-05）**：

| 场景 | 命令 |
|---|---|
| 早上开工，看昨天改了什么 | `git show HEAD` |
| 刚提交完发现漏了文件 / 信息写错 | `git add <文件>` + `git commit --amend` |
| 改乱了，丢掉回到上次提交 | `git restore <文件>`（不可逆，先确认） |
| 最近几次提交各动了哪些文件 | `git log --stat -3` |
| 某个文件的演变历史 | `git log -p src/net/Buffer.cpp` |

不带文件名的 `git diff` 会进分页器：**空格翻页，q 退出**；`git --no-pager diff` 一次全出。管道里第二个命令不用加 `rtk`。

## 二、踩坑记录

| 日期 | 坑 | 结论 / 状态 |
|---|---|---|
| 2026-08 | WSL mirrored 网络模式：连**空端口**不报 `Connection refused`，而是永久卡死 | 调试一律加 `timeout`，否则分不清"没起来"和"回错了" |
| 2026-08 | redis-cli 7.0.15 没有 `-t` 超时参数 | 用 shell 的 `timeout 2 redis-cli ...` |
| 2026-08 | 交互模式 redis-cli 连 v0 会卡死：一连上先发 `COMMAND DOCS` | **原因待答**——见 `PROGRESS.md` 待答的题 #1 |
| 2026-09-07 | CMakeLists 把 `MemStore.cpp` 打成 `MemStore.cp` | **文件路径拼错 CMake 会报错**（磁盘上查得到），**变量名拼错它不吭声**（变量不用声明）。"会不会提醒你"取决于它有没有办法知道你错了 |
| 2026-09-07 | 命名空间改名后 `main.cpp` 改干净了，`tests/parse_test.cpp` 9 处漏改 | **改名只 build 主 target 会一路绿着骗过你**——测试是独立 target，不参与就不报错。**改完名必须全量 build + ctest** |
| 2026-09-09 | CMakeLists 里写 `commands.cpp` 而不是 `src/cmd/commands.cpp` | `add_executable` 的路径**相对于 CMakeLists.txt 所在目录**（项目根），和 `target_include_directories` 无关——后者只管 `#include` 怎么找头文件 |
| 2026-09-09 | 起服务报 `bind failed: Address already in use` | 旧进程还活着占着端口。**`SO_REUSEADDR` 放宽的是"绑到 TIME_WAIT 的旧连接上"，不是"和活着的监听 socket 抢端口"**。习惯：起服务前先 `ss -ltn \| grep 6380` |
| 2026-09-09 | `contains` 又飘红 | IntelliSense 按 C++17 猜（实测 C++17 报错、C++20 通过）。`build/compile_commands.json` 里记的是 `-std=gnu++20`，但没人告诉 VSCode 去读它 → 建 `.vscode/c_cpp_properties.json` 指过去。**`.vscode/` 在 .gitignore 里，换机器/清目录就会丢** |
| 2026-09-09 | 改命名空间后 `main.cpp` 干净了，测试没跟着改 | 只 build 主 target 会一路绿着骗过你。**改完名必须全量 build + ctest** |
