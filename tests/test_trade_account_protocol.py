"""New identity protocol checks; PROTOC must point to protoc 29.3."""
import os
import re
import subprocess
import tempfile
import unittest
from pathlib import Path
from google.protobuf import descriptor_pb2
ROOT=Path(__file__).resolve().parents[2]
PROTOC=os.environ.get('PROTOC','protoc')
PROJECTS=('XBroker','XEngine','XTrader','HQMarket')
def command(project,action):
    folder=ROOT/project/project/'request'
    return [PROTOC,'-I',str(folder),'--'+action+'=request.RequestData',str(folder/'request.proto')]
class IdentityProtocolTest(unittest.TestCase):
    def test_all_schemas_and_request_enums_match(self):
        descriptors=[]
        for project in PROJECTS:
            folder=ROOT/project/project/'request'
            with tempfile.TemporaryDirectory() as directory:
                path=Path(directory)/'schema.pb'
                subprocess.run([PROTOC,'-I',str(folder),'--include_imports','--descriptor_set_out='+str(path),str(folder/'request.proto')],check=True)
                descriptors.append(path.read_bytes())
            descriptor=descriptor_pb2.FileDescriptorSet.FromString(descriptors[-1])
            enum=next(e for f in descriptor.file for e in f.enum_type if e.name=='RequestType')
            text=(folder/'request.h').read_text(encoding='utf-8')
            body=re.search(r'enum class types\s*\{(.*?)\};',text,re.S).group(1)
            self.assertEqual({v.name:v.number for v in enum.value},{n:int(v) for n,v in re.findall(r'(\w+)\s*=\s*(\d+)',body)})
            messages=[m for f in descriptor.file for m in f.message_type]
            for message in messages:
                self.assertNotIn('account_id',[field.name for field in message.field])
            self.assertNotIn('TradeAccountType',[e.name for f in descriptor.file for e in f.enum_type])
            self.assertNotIn('TradeAccountData',[m.name for m in messages])
        self.assertTrue(all(d==descriptors[0] for d in descriptors))
    def test_leading_zero_and_separate_identity_fields(self):
        text='type: TRADE_ACCOUNT_LIST_QUERY account_list { accounts { broker_id: 2 site: "other" account: "000123" broker_name: "Broker" name: "Name" } }'
        encoded=subprocess.run(command('XBroker','encode'),input=text.encode(),capture_output=True,check=True).stdout
        for project in PROJECTS:
            decoded=subprocess.run(command(project,'decode'),input=encoded,capture_output=True,check=True).stdout.decode()
            self.assertIn('account: "000123"',decoded)
            self.assertIn('broker_id: 2',decoded)
            self.assertIn('site: "other"',decoded)
    def test_no_password_in_public_account(self):
        for project in PROJECTS:
            schema=(ROOT/project/project/'request/request.proto').read_text(encoding='utf-8')
            account=re.search(r'message TradeAccountRecord \{(.*?)\}',schema,re.S).group(1)
            self.assertNotIn('password',account)
            self.assertNotIn('adapter',account)
if __name__=='__main__':unittest.main()
