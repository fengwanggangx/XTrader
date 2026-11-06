# Strategy management regression tests

Run from an MSVC developer shell with CMake and Qt 6 available:

```powershell
cmake -S XTrader/tests/strategy -B .build/strategy-management-tests -DCMAKE_PREFIX_PATH=D:/Qt/6.11.2/msvc2022_64
cmake --build .build/strategy-management-tests --config Debug
$env:PATH = "D:/Qt/6.11.2/msvc2022_64/bin;D:/codex/CS/XTrader/XTrader/libs/debug;$env:PATH"
ctest --test-dir .build/strategy-management-tests -C Debug --output-on-failure
```

Adjust the Qt and repository paths for the local installation. The test replaces
session transport and account storage with local fakes; it sends no network
requests. Production strategy service, editor, and strategy management view are
compiled directly. Coverage includes invalid subscriptions and parameters,
runtime edit guards, stale in-flight replies, disconnect and timeout recovery,
account identity preservation, modal deletion refresh, and queued UI invalidation.
