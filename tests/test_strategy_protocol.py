"""Cross-service strategy wire contracts; PROTOC must point to protoc 29.3."""
import os
import re
import subprocess
import tempfile
import unittest
from pathlib import Path

from google.protobuf import descriptor_pb2

ROOT = Path(__file__).resolve().parents[2]
PROJECTS = ("XEngine", "XTrader", "XBroker", "HQMarket")


class StrategyProtocolTest(unittest.TestCase):
    def test_strategy_contracts_match_across_services(self):
        shared = {}
        for project in PROJECTS:
            folder = ROOT / project / project / "request"
            with tempfile.TemporaryDirectory() as directory:
                output = Path(directory) / "schema.pb"
                subprocess.run([
                    os.environ.get("PROTOC", "protoc"), "-I" + str(folder),
                    "--include_imports", "--descriptor_set_out=" + str(output),
                    str(folder / "request.proto"),
                ], check=True)
                schema = descriptor_pb2.FileDescriptorSet.FromString(output.read_bytes())
            request_enum = next(e for f in schema.file for e in f.enum_type if e.name == "RequestType")
            wire_types = {value.name: value.number for value in request_enum.value}
            header = (folder / "request.h").read_text(encoding="utf-8")
            body = re.search(r"enum class types\s*\{(.*?)\};", header, re.S).group(1)
            self.assertEqual(wire_types, {name: int(value) for name, value in re.findall(r"(\w+)\s*=\s*(\d+)", body)}, project)
            self.assertEqual(28, wire_types["STRATEGY_ACTION"])
            self.assertEqual(67, wire_types["TRADE_EXECUTION_RULES_QUERY"])
            messages = {message.name: message for file in schema.file for message in file.message_type}
            for name in ("ExecutionRules", "StrategyInfo", "StrategyRuntimeRecord", "BarData"):
                data = messages[name].SerializeToString()
                self.assertEqual(shared.setdefault(name, data), data, project + ":" + name)
            fields = {field.name for field in messages["BarData"].field}
            self.assertTrue({"completed", "end_time_ms", "data_version", "basis_version"}.issubset(fields))

    def test_client_and_engine_share_parameter_validation(self):
        engine = ROOT / "XEngine/XEngine/strategy/StrategySettings.h"
        client = ROOT / "XTrader/XTrader/service/strategy/StrategySettings.h"
        self.assertEqual(
            engine.read_text(encoding="utf-8").replace("../common/utility.h", "common/utility.h"),
            client.read_text(encoding="utf-8").replace("../../common/utility.h", "common/utility.h"),
        )


if __name__ == "__main__":
    unittest.main()
