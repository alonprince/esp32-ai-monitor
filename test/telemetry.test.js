import { describe, it, expect } from 'vitest'

// Helper function to simulate active_tool parser logic in JS
function parseTelemetryTasks(activeToolStr) {
  if (!activeToolStr || activeToolStr === 'None' || activeToolStr === 'idle') {
    return []
  }
  return activeToolStr.split('|').map(taskStr => {
    const [name, id, time, status] = taskStr.split(',')
    return {
      name: name || 'Unknown Task',
      id: id || '---',
      time: time || '00:00',
      status: status || 'idle'
    }
  })
}

describe('BLE Telemetry Parser Logic', () => {
  it('should return empty list when telemetry is empty or None', () => {
    expect(parseTelemetryTasks('')).toEqual([])
    expect(parseTelemetryTasks('None')).toEqual([])
  })

  it('should parse single task correctly', () => {
    const raw = 'Model Training,99x-A,03:32,working'
    const result = parseTelemetryTasks(raw)
    expect(result).toHaveLength(1)
    expect(result[0]).toEqual({
      name: 'Model Training',
      id: '99x-A',
      time: '03:32',
      status: 'working'
    })
  })

  it('should parse multiple delimited tasks correctly', () => {
    const raw = 'Model Training,99x-A,03:32,working|Data Indexing,102-B,01:15,done'
    const result = parseTelemetryTasks(raw)
    expect(result).toHaveLength(2)
    expect(result[0].name).toBe('Model Training')
    expect(result[1].name).toBe('Data Indexing')
    expect(result[1].status).toBe('done')
  })
})
