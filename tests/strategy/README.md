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

## Four-strategy implementation checks

Build the dedicated targets to validate the four strategy forms and compile the production strategy view:

```powershell
cmake --build .build/strategy-management-tests --config Debug --target FourStrategyUiTests strategy_view_compile
ctest --test-dir .build/strategy-management-tests -C Debug -R FourStrategyUiTests --output-on-failure
```

These checks cover typed parameters, thresholds, generated subscriptions, immutable account/type/security binding, configuration revision and action response routing. They use production dialog/service code with local transport and account fixtures. The older `StrategyManagementTests` target is separate and has not been validated for the new forms.

Cross-service protocol checks are in `../test_strategy_protocol.py`; run with Python protobuf installed and `PROTOC` pointing to protoc 29.3. They compare shared strategy messages across four projects, local request enums and the shared client/engine parameter validator.
