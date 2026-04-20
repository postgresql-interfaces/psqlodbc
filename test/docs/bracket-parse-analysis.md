# drvconn.c 括号解析逻辑分析与 DT 测试用例设计

## 一、目标代码位置

- **文件**: `drvconn.c`
- **函数**: `dconn_get_attributes()`
- **行号**: 510–573
- **涉及常量** (`drvconn.c:443-445`):
  ```c
  #define ATTRIBUTE_DELIMITER ';'
  #define OPENING_BRACKET     '{'
  #define CLOSING_BRACKET     '}'
  ```

## 二、功能描述

### 2.1 整体作用

`dconn_get_attributes()` 负责解析 ODBC 连接字符串（形如 `KEY1=VAL1;KEY2=VAL2;...`），并把每个 `KEY=VAL` 对交给回调 `func` 写入 `ConnInfo`。

- 使用 `strtok_r`/`strtok` 以分号 `;` 作为分隔符切分字符串。`strtok` 会把原串中的 `;` 替换为 `\0`（破坏性修改）。
- 当 value 被 `{...}` 花括号包裹时，值内部允许出现 `;`（例如密码中含有分号），此时必须"还原"被 `strtok` 误切掉的 `;`，继续查找真正的闭合括号。
- 花括号本身不会在这里被剥离，真正的去括号逻辑在 `dlg_specific.c::decode_or_remove_braces()` 中完成。

### 2.2 关键变量

| 变量 | 类型 | 含义 |
|------|------|------|
| `our_connect_string` | `char*` | `strdup` 得到的连接串副本，可被破坏性修改 |
| `termp` | `const char*` | 指向整串末尾 `\0`，作为越界判断上限 |
| `strtok_arg` | `char*` | 下一次调用 `strtok` 的输入；初始为完整串，之后为 `NULL` 或指向续点 |
| `pair` | `const char*` | 当前 `strtok` 返回的子串（形如 `KEY=VAL`） |
| `equals` | `char*` | `pair` 中 `=` 的位置，置 `\0` 分割 attribute/value |
| `value` | `const char*` | `=` 之后的值起点 |
| `delp` | `char*` | 当前值所在 `strtok` token 的尾部 `\0` 位置。等价于"被 `strtok` 替换掉的 `;` 的原始位置" |
| `closep` | `const char*` | `}` 出现的位置 |
| `valuen` | `const char*` | 为下一轮 `strchr` 定位的推进指针 |
| `eoftok` | `BOOL` | 是否已到达整串尾，是则 while 循环结束 |

### 2.3 控制流

`switch (*value)` 只处理两种情形：

1. **`case OPENING_BRACKET` (值以 `{` 开头)** — 需要特殊的闭合括号搜索。
2. **default** — 不做任何处理，继续向下执行 `(*func)(ci, attribute, value)`。

下图说明 `case OPENING_BRACKET` 内的分支结构：

```
case '{':
  delp = strchr(value, '\0');              // 当前 token 末尾
  if (delp >= termp) { eoftok = TRUE; break; }   // B0: 已到整串结尾

  closep = strchr(value, '}');
  if (closep && closep[1] == '\0')         // B1: 一次命中, 且其后是整串结尾
      break;

  for (valuen = value; valuen < termp; closep = strchr(valuen, '}'))
  {
      if (closep == NULL) {                // B2: 当前分段无 '}'
          if (!delp) { ret = FALSE; goto cleanup; }     // B2a: 错误1
          closep = strchr(delp + 1, '}');
          if (!closep) { ret = FALSE; goto cleanup; }   // B2b: 错误2
          *delp = ';';                     // 还原被 strtok 吃掉的分号
          delp = NULL;
      }
      if (closep[1] == '}') {              // B3: "}}" 转义
          valuen = closep + 2;
          if (valuen >= termp) break;      // B3a: 到整串结尾
          else if (valuen == delp) {       // B3b: 撞到已存在的 token 边界
              *delp = ';';
              delp = NULL;
          }
          continue;
      }
      else if (closep[1] == ';' ||         // B4: 合法闭合括号
               closep[1] == '\0' ||
               delp == closep + 1) {
          delp = closep + 1;
          *delp = '\0';                    // 切断 value
          strtok_arg = delp + 1;           // 下一轮 strtok 从此处
          if (strtok_arg + 1 >= termp) eoftok = TRUE;
          break;
      }
      // B5: '}' 后是异常字符, 报错
      ret = FALSE; goto cleanup;
  }
```

### 2.4 分支清单（DT 覆盖矩阵）

| 分支 ID | 触发条件 | 结果 |
|---------|---------|------|
| B0 | `value` 为 `{`, 紧邻整串结尾 | `eoftok = TRUE` |
| B1 | `{value}` 完整且后续是整串结尾 | 直接 `break` |
| B2a | `strtok` 分段中无 `}`，且 `delp == NULL` | goto cleanup (error 1) |
| B2b | 试图跨段查找 `}` 仍失败 | goto cleanup (error 2) |
| B2 恢复 | 跨段成功找到 `}`，还原 `;` | 继续循环 |
| B3 | 出现 `}}` 转义 | `valuen += 2`, `continue` |
| B3a | `}}` 后就是整串结尾 | `break` |
| B3b | `valuen == delp` 恰好落在段边界 | 还原 `;` |
| B4 | `}` 后面是 `;` / `\0` / 段边界 | 合法闭合, 推进 `strtok_arg` |
| B4-eof | 合法闭合后剩余不足一个 token | `eoftok = TRUE` |
| B5 | `}` 后是其他字符 | goto cleanup (error 3) |
| default | `*value != '{'` | 走默认流程 |

## 三、潜在缺陷/风险点

1. **指针算术越界风险**: `strtok_arg + 1 >= termp` 的比较中 `strtok_arg + 1` 在 `strtok_arg` 指向最后一个字节时可能越界 1（未解引用则为合法 C），若后续代码依赖值则需谨慎。
2. **`delp` 指针的生命周期**: `delp` 初始为 `strchr(value, '\0')`，在 `strtok` 重复调用后可能指向已被其他逻辑写入 `;` 又置为 `\0` 的位置，与 `termp` 的比较逻辑在多次循环叠加下容易出错。
3. **`closep = strchr(delp + 1, ...)`**: 依赖 `delp + 1` 仍在 `our_connect_string` 内部，如果 `delp` 已经等于 `termp - 1`，`delp + 1` 指向字符串末尾 `\0`，`strchr` 会立即返回 `NULL`（合法），但若 `delp` 被误赋值为 `NULL` 之外的非法位置将 crash。
4. **代码缩进陷阱**: 第 523–525 行 `if (... closep[1] == '\0') break;` 的 `break` 未加大括号，实际上只对 `break` 生效，容易让阅读者误判作用域（但语义正确）。
5. **`}}` 嵌套计数不完整**: 代码只识别 `}}` 转义，不识别嵌套 `{`；连续的 `{{...}}` 逻辑是否与 `decode_or_remove_braces()` 完全匹配需要测试交叉验证。
6. **大字符串栈空间**: 拷贝前 `strdup`，长度取决于调用者传入串长度。DT 应覆盖超长值，避免与上层缓冲区交互时产生截断。
7. **空值 `{}`**: 长度为 0 的括号是否能被 `decode_or_remove_braces()` 正常处理需要端到端验证。

## 四、DT 测试用例设计

测试文件: `test/src/bracket-parse-test.c`（20 个用例）。

### 4.1 覆盖矩阵

| TC  | 描述 | 主要分支 | 预期 |
|-----|------|---------|------|
| TC01 | `PWD={simple_password}` 普通括号值 | B1 → (*func) | 不 crash，连接正常或报 DSN 级错误 |
| TC02 | `PWD={pass;word;123}` 括号内含分号 | B2→恢复→B4 | 不 crash |
| TC03 | `PWD={lastvalue}` 括号值位于整串末尾 | B0 / B1 | 不 crash，`eoftok` 路径 |
| TC04 | `PWD={pass}}word}` 转义 `}}` | B3 → B4 | 不 crash |
| TC05 | `PWD={a}}b}}c}}` 多次转义 | B3 多轮 | 不 crash |
| TC06 | `PWD={no_close;Server=host` 缺闭合 | B2a | 返回错误但不 crash |
| TC07 | `PWD={no_close_at_all` 完全缺闭合 | B2b | 返回错误但不 crash |
| TC08 | `PWD={value}x;...` `}` 后非法字符 | B5 | 返回错误但不 crash |
| TC09 | `PWD={};Server=host` 空括号 | B4 | 不 crash |
| TC10 | 括号在末 token, `closep[1]=='\0'` | B1 | 不 crash |
| TC11 | `PWD={semi;colon;in;value};...` 跨多 strtok 段 | B2→恢复→B4 | 不 crash |
| TC12 | `PWD={val}};more}` 转义紧贴段边界 | B3b | 不 crash |
| TC13 | `PWD={value};` `delp==closep+1` | B4 | 不 crash |
| TC14 | 长度 2KB 的括号值 | 缓冲区压力 | 不 crash |
| TC15 | `PWD={` 只有左括号 | B0 | 不 crash |
| TC16 | `PWD={x}}` 转义后即整串结尾 | B3a | 不 crash |
| TC17 | `PWD={val};` 解析后 `strtok_arg+1>=termp` | B4-eof | 不 crash |
| TC18 | 多个括号值串联 | B4 多轮 | 不 crash |
| TC19 | `PWD={=;{nested};==}` 特殊字符 | 多分支叠加 | 不 crash |
| TC20 | `PWD=plaintext` 基线（不走括号） | default | 不 crash |

### 4.2 测试通过判定

- **核心目标**: 无 core dump / 无段错误 / 无 stack overflow（加固型 DT，聚焦稳定性）。
- **次要目标**: 错误路径（TC06/07/08）应返回 SQL_ERROR 或 SQL_SUCCESS_WITH_INFO，并带有合理 SQLSTATE。
- 所有 TC 输出 `[No crash - PASS]` 即视为用例通过。

### 4.3 断言策略

- 测试程序捕获 `SQLDriverConnect` 返回值，**不**因错误退出，逐个用例继续执行。
- 即使连接成功，也立即 `SQLDisconnect` 释放资源，避免污染后续 TC。
- 所有句柄都用本地变量，避免与 `common.c` 的 `env/conn` 全局变量产生干扰。

## 五、如何在服务器上运行测试

### 5.1 源文件清单

测试源文件已经存在:
```
test/src/bracket-parse-test.c
```

### 5.2 把测试注册到构建系统

**步骤 1**: 编辑 `test/tests`，在 `TESTBINS` 列表末尾（`exe/primarykeys-include-test` 之后）追加一行：

```
TESTBINS = exe/connect-test \
    ...
    exe/primarykeys-include-test \
    exe/bracket-parse-test
```

注意最后一行**不要**加反斜杠续行符。

**步骤 2**: 创建期望输出文件 `test/expected/bracket-parse.out`。因为本测试关注"不 crash"，你可以选择以下两种策略之一：

- **策略 A (推荐)**: 把第一次运行的真实输出 copy 到 expected 目录作为基线，后续回归只需保持输出稳定；
- **策略 B**: 直接把所有 `[No crash - PASS]` 行作为期望基线。

### 5.3 Linux / Unix 服务器执行流程

假设源码位于 `/data/psqlodbc`，已有 GaussDB/PostgreSQL 环境。

```bash
# 1. 进入源码目录
cd /data/psqlodbc

# 2. 配置
./bootstrap
./configure --with-libpq=/path/to/libpq --enable-pgport=5432

# 3. 构建驱动
make

# 4. 安装驱动到系统 (如果首次)
sudo make install

# 5. 构建测试
cd test
make

# 6. 准备 DSN (odbc.ini 会由 odbcini-gen.sh 生成; 可通过环境变量指定目标 DSN)
export PSQLODBC_TEST_DSN=psqlodbc_test_dsn

# 7. 单独跑括号解析测试 (不走 runsuite 对比, 直接看输出)
./exe/bracket-parse-test

# 8. 或通过 runsuite 走完整对比 (需要 expected/bracket-parse.out)
./runsuite bracket-parse --inputdir=.

# 9. 或纳入整套回归
make installcheck
```

### 5.4 Windows 服务器执行流程

```cmd
REM 在 psqlodbc 根目录
nmake /f windows-makefile.mak CPU=x64 CFG=Release

REM 构建测试
cd test
nmake /f win.mak

REM 单独跑
exe\bracket-parse-test.exe

REM 走完整回归
nmake /f win.mak installcheck
```

### 5.5 带 Sanitizer 的稳定性加固跑法（推荐）

对 DT 加固任务，建议启用 AddressSanitizer / UBSan 捕获潜在越界/未定义行为：

```bash
# 1. 用 ASan/UBSan 编译驱动和测试
./configure CFLAGS="-O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined" \
            LDFLAGS="-fsanitize=address,undefined"
make clean && make
cd test && make

# 2. 运行时开启相关选项
export ASAN_OPTIONS=halt_on_error=1:abort_on_error=1:detect_leaks=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
./exe/bracket-parse-test
```

### 5.6 Valgrind 内存检查跑法（可选）

```bash
valgrind --error-exitcode=1 --leak-check=full --track-origins=yes \
    ./exe/bracket-parse-test
```

### 5.7 压力放大（发现偶发 core）

```bash
# 循环执行, 一旦 core 立即停止
for i in $(seq 1 1000); do
    ./exe/bracket-parse-test || { echo "FAIL at iter $i"; break; }
done

# 或并发跑多个实例检测多线程下 strtok 相关问题
for i in $(seq 1 16); do ./exe/bracket-parse-test & done; wait
```

### 5.8 输出判读

- 成功: 每个用例输出 `[No crash - PASS]`，末尾 `=== All tests completed without crash ===`。
- 失败: 若中途 core dump，shell 会打印 `Segmentation fault` / `Aborted (core dumped)`；检查 `/var/lib/systemd/coredump/` 或 `ulimit -c unlimited` 开启的工作目录下的 core 文件。
- ASan 触发: stderr 出现 `==xxxx==ERROR: AddressSanitizer: ...`，附带完整调用栈。

## 六、后续扩展建议

- 加入 Fuzz 用例: 使用 libFuzzer/AFL 对 `dconn_get_attributes` 做封装做 fuzz，覆盖更多随机括号组合。
- 把 `bracket-parse-test` 纳入 CI 必跑项，配合 Sanitizer 构建变体。
- 针对 B5（`}` 后异常字符）增加多字符集/CJK 字节序列用例，验证 ANSI 版本驱动在 GBK/UTF-8 下的行为一致。
