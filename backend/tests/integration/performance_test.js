const fs = require('fs');
const path = require('path');
const fetch = require('node-fetch');

const API_BASE_URL = process.env.API_BASE_URL || 'http://localhost:8080/api/v1';

async function callRouteGenerate() {
    const start = Date.now();
    try {
        const response = await fetch(`${API_BASE_URL}/route/generate`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                start_point: { lat: 35.681236, lon: 139.767125 },
                end_point: { lat: 35.685175, lon: 139.752799 }
            })
        });
        const duration = Date.now() - start;
        return { status: response.status, duration };
    } catch (error) {
        return { status: 500, duration: Date.now() - start, error: error.message };
    }
}

async function runConcurrentTest(concurrency) {
    console.log(`\n- Running Concurrent Test: ${concurrency} requests`);
    const promises = [];
    for (let i = 0; i < concurrency; i++) {
        promises.push(callRouteGenerate());
    }
    const results = await Promise.all(promises);
    const success = results.filter(r => r.status === 200).length;
    const durations = results.map(r => r.duration).sort((a, b) => a - b);
    const avg = durations.reduce((a, b) => a + b, 0) / durations.length;
    const p95 = durations[Math.floor(durations.length * 0.95)];
    
    console.log(`  Success: ${success}/${concurrency}`);
    console.log(`  Avg Latency: ${avg.toFixed(2)}ms`);
    console.log(`  P95 Latency: ${p95}ms`);
}

async function runDurationTest(durationMs, intervalMs) {
    console.log(`\n- Running Duration Test: ${durationMs}ms at ${intervalMs}ms interval`);
    const startTime = Date.now();
    let count = 0;
    let success = 0;
    
    while (Date.now() - startTime < durationMs) {
        const res = await callRouteGenerate();
        count++;
        if (res.status === 200) success++;
        await new Promise(resolve => setTimeout(resolve, intervalMs));
    }
    
    console.log(`  Total Requests: ${count}`);
    console.log(`  Success: ${success}/${count}`);
}

async function runConsistencyTest(rounds) {
    console.log(`\n- Running Consistency Test: ${rounds} rounds`);
    const durations = [];
    for (let i = 0; i < rounds; i++) {
        const res = await callRouteGenerate();
        if (res.status === 200) durations.push(res.duration);
    }
    const avg = durations.reduce((a, b) => a + b, 0) / durations.length;
    const min = Math.min(...durations);
    const max = Math.max(...durations);
    console.log(`  Avg: ${avg.toFixed(2)}ms, Min: ${min}ms, Max: ${max}ms`);
}

async function runAllPerformanceTests() {
    console.log('--- Starting Performance Tests ---');
    
    await runConcurrentTest(10);
    await runConcurrentTest(50);
    
    await runDurationTest(10000, 100); // 10 seconds, 100ms interval
    
    await runConsistencyTest(20);
    
    console.log('\n--- Performance Tests Completed ---');
}

runAllPerformanceTests();
