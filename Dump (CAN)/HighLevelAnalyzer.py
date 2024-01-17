# High Level Analyzer
# For more information and documentation, please go to https://support.saleae.com/extensions/high-level-analyzer-extensions

from saleae.analyzers import HighLevelAnalyzer, AnalyzerFrame, StringSetting, ChoicesSetting
import json
import os

class CANMessage:
    def __init__(self, id, timestamp):
        self.id = id
        self.data = []
        self.timestamp = timestamp

# High level analyzers must subclass the HighLevelAnalyzer class.
class Hla(HighLevelAnalyzer):
    # List of settings that a user can set for this High Level Analyzer.
    dump_path = StringSetting()
    overwrite = ChoicesSetting(choices=('Yes', 'No'))
    
    def __init__(self):
        self.current_message = None
        if self.overwrite == 'Yes' and os.path.exists(self.dump_path):
            os.remove(self.dump_path)
        self._file = open(self.dump_path, 'w')
        
    def decode(self, frame: AnalyzerFrame):
        #{'type': 'identifier_field', 'start_time': SaleaeTime(datetime.datetime(2024, 1, 12, 15, 51, 25, tzinfo=datetime.timezone.utc), millisecond=68.274125), 'end_time': SaleaeTime(datetime.datetime(2024, 1, 12, 15, 51, 25, tzinfo=datetime.timezone.utc), millisecond=68.334125), 'data': {'identifier': 768}}
        #{'type': 'data_field', 'start_time': SaleaeTime(datetime.datetime(2024, 1, 12, 15, 51, 25, tzinfo=datetime.timezone.utc), millisecond=68.354125), 'end_time': SaleaeTime(datetime.datetime(2024, 1, 12, 15, 51, 25, tzinfo=datetime.timezone.utc), millisecond=68.386125), 'data': {'data': '03'}}
        #{'type': 'ack_field', 'start_time': SaleaeTime(datetime.datetime(2024, 1, 12, 15, 51, 25, tzinfo=datetime.timezone.utc), millisecond=68.686125), 'end_time': SaleaeTime(datetime.datetime(2024, 1, 12, 15, 51, 25, tzinfo=datetime.timezone.utc), millisecond=68.690125), 'data': {'ack': True}}
        if frame.type == 'identifier_field':
            self.current_message = CANMessage(frame.data['identifier'], frame.start_time.as_datetime().timestamp())
        elif frame.type == 'data_field':
            self.current_message.data.append(frame.data['data'].hex())
            if self.current_message.id == 768:
                print(frame.data['data'].hex(), end=' ')
        elif frame.type == 'ack_field':
            json_string = json.dumps(self.current_message.__dict__)
            self._file.write(json_string + '\n')
            if self.current_message.id == 768:
                print()
                print(json_string)
            self.current_message = None

    def __del__(self):
        self._file.close()
