import unittest

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

if __name__ == '__main__':
    unittest.main()
