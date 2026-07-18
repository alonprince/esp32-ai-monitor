import unittest
import os
import json
import tempfile
import datetime
import mac_host


def parse_telemetry_tasks(active_tool_str):
    if not active_tool_str or active_tool_str == 'None' or active_tool_str == 'idle':
        return []
    tasks = []
    for task_str in active_tool_str.split('|'):
        parts = task_str.split(',')
        while len(parts) < 4:
            parts.append('')
        name, task_id, time, status = parts[0], parts[1], parts[2], parts[3]
        tasks.append({
            'name': name or 'Unknown Task',
            'id': task_id or '---',
            'time': time or '00:00',
            'status': status or 'idle'
        })
    return tasks

class TestBLETelemetryParser(unittest.TestCase):
    def test_empty_or_none(self):
        self.assertEqual(parse_telemetry_tasks(''), [])
        self.assertEqual(parse_telemetry_tasks('None'), [])
        self.assertEqual(parse_telemetry_tasks('idle'), [])

    def test_parse_single_task(self):
        raw = 'Model Training,99x-A,03:32,working'
        result = parse_telemetry_tasks(raw)
        self.assertEqual(len(result), 1)
        self.assertEqual(result[0], {
            'name': 'Model Training',
            'id': '99x-A',
            'time': '03:32',
            'status': 'working'
        })

    def test_parse_multiple_tasks(self):
        raw = 'Model Training,99x-A,03:32,working|Data Indexing,102-B,01:15,done'
        result = parse_telemetry_tasks(raw)
        self.assertEqual(len(result), 2)
        self.assertEqual(result[0]['name'], 'Model Training')
        self.assertEqual(result[1]['name'], 'Data Indexing')
        self.assertEqual(result[1]['status'], 'done')

class TestMacHostChanges(unittest.TestCase):
    def setUp(self):
        # Store original CODEX_SESSIONS_DIR and restore in tearDown
        self.original_sessions_dir = mac_host.CODEX_SESSIONS_DIR
        self.temp_dir = tempfile.TemporaryDirectory()
        mac_host.CODEX_SESSIONS_DIR = self.temp_dir.name

    def tearDown(self):
        mac_host.CODEX_SESSIONS_DIR = self.original_sessions_dir
        self.temp_dir.cleanup()

    def test_clean_chinese_to_pinyin(self):
        # Verify that clean_chinese_to_pinyin now returns the text unchanged
        text = "测试中文 Pinyin 123"
        self.assertEqual(mac_host.clean_chinese_to_pinyin(text), text)

    def test_get_codex_rate_limit_remaining_empty(self):
        # Verify default return when no files are present
        pct, reset_str = mac_host.get_codex_rate_limit_remaining()
        self.assertEqual(pct, 100)
        self.assertEqual(reset_str, "--")

    def test_get_codex_rate_limit_remaining_no_token_count(self):
        # Write a jsonl with no token_count event
        session_file = os.path.join(mac_host.CODEX_SESSIONS_DIR, "session1.jsonl")
        with open(session_file, "w") as f:
            f.write(json.dumps({"timestamp": "2026-07-18T12:00:00Z", "payload": {"type": "other"}}) + "\n")
        
        pct, reset_str = mac_host.get_codex_rate_limit_remaining()
        self.assertEqual(pct, 100)
        self.assertEqual(reset_str, "--")

    def test_get_codex_rate_limit_remaining_valid(self):
        # Write a valid jsonl file with a token_count payload
        session_file = os.path.join(mac_host.CODEX_SESSIONS_DIR, "session2.jsonl")
        
        resets_at_ts = 1784700000
        dt = datetime.datetime.fromtimestamp(resets_at_ts)
        MONTHS = ["Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"]
        expected_reset_str = f"{dt.day} {MONTHS[dt.month-1]} {dt.hour:02d}:{dt.minute:02d}"

        payload_data = {
            "timestamp": "2026-07-18T13:00:00Z",
            "payload": {
                "type": "token_count",
                "rate_limits": {
                    "primary": {
                        "used_percent": 30.5,
                        "resets_at": resets_at_ts
                    }
                }
            }
        }
        
        with open(session_file, "w") as f:
            f.write(json.dumps(payload_data) + "\n")
            
        pct, reset_str = mac_host.get_codex_rate_limit_remaining()
        self.assertEqual(pct, 70)  # 100 - 30 = 70
        self.assertEqual(reset_str, expected_reset_str)

class MockBLECommandProcessor:
    def __init__(self):
        self.calls = []

    async def process_command(self, cmd, **kwargs):
        self.calls.append((cmd, kwargs))

class TestHandleJsonCommand(unittest.IsolatedAsyncioTestCase):
    async def test_handle_event_hook(self):
        processor = MockBLECommandProcessor()
        # Mock CODEX_SESSIONS_DIR to point to empty dir
        original_sessions_dir = mac_host.CODEX_SESSIONS_DIR
        
        # Mock SQLite query function to be independent of local DB state
        original_info_func = mac_host.get_latest_codex_session_info
        mac_host.get_latest_codex_session_info = lambda: {"preview": "Test Task", "id": "12345678"}

        try:
            with tempfile.TemporaryDirectory() as temp_dir:
                mac_host.CODEX_SESSIONS_DIR = temp_dir
                cmd_data = {
                    "cmd": "codeisland_payload",
                    "payload": {
                        "event": "PreToolUse",
                        "task_name": "Test Task",
                        "session_id": "opencode-12345678",
                        "tool": "git"
                    }
                }
                await mac_host.handle_json_command(processor, json.dumps(cmd_data))
                
                # Assert calls received
                cmds = [c[0] for c in processor.calls]
                self.assertIn("state", cmds)
                self.assertIn("tool", cmds)
                self.assertIn("preview", cmds)
                self.assertIn("stats", cmds)
                self.assertIn("quota_reset", cmds)
                
                # Verify the tool format
                tool_call = [c for c in processor.calls if c[0] == "tool"][0]
                self.assertEqual(tool_call[1]["value"], "Test Task (git),12345678,00:00,working")
                
                # Verify default stats/quota reset are sent
                stats_call = [c for c in processor.calls if c[0] == "stats"][0]
                self.assertEqual(stats_call[1]["codex"], 100)
                
                quota_call = [c for c in processor.calls if c[0] == "quota_reset"][0]
                self.assertEqual(quota_call[1]["value"], "--")
        finally:
            mac_host.get_latest_codex_session_info = original_info_func
            mac_host.CODEX_SESSIONS_DIR = original_sessions_dir


    async def test_handle_direct_command(self):
        processor = MockBLECommandProcessor()
        cmd_data = {
            "cmd": "stats",
            "codex": 55
        }
        await mac_host.handle_json_command(processor, json.dumps(cmd_data))
        
        self.assertEqual(len(processor.calls), 1)
        self.assertEqual(processor.calls[0], ("stats", {"codex": 55}))

    def test_safe_truncate_utf8(self):
        # Create a value that would be truncated in the middle of a 3-byte Chinese character
        value_chars = "a" * 254 + "哈"
        encoded = value_chars.encode('utf-8')
        truncated = mac_host.encode_tlv(0x02, encoded)

        parsed_val = truncated[2:] # skip type and length
        self.assertEqual(len(parsed_val), 254) # "哈" should be dropped completely since it was cut in half
        self.assertEqual(parsed_val.decode('utf-8'), "a" * 254)

if __name__ == '__main__':
    # Import mac_host and related modules needed for tests
    import tempfile
    import datetime
    import mac_host
    unittest.main()


