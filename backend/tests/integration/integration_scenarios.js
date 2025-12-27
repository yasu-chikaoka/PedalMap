const fs = require('fs');
const path = require('path');
const fetch = require('node-fetch');

const API_BASE_URL = process.env.API_BASE_URL || 'http://localhost:8080/api/v1';

async function callRouteGenerate(payload) {
    try {
        const response = await fetch(`${API_BASE_URL}/route/generate`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload)
        });
        const status = response.status;
        let data = null;
        try {
            data = await response.json();
        } catch (e) {
            data = await response.text();
        }
        return { status, data };
    } catch (error) {
        return { status: 500, error: error.message };
    }
}

async function runTests() {
    const testRoutes = JSON.parse(fs.readFileSync(path.join(__dirname, 'data/test_routes.json'), 'utf8'));
    const invalidInputs = JSON.parse(fs.readFileSync(path.join(__dirname, 'data/invalid_inputs.json'), 'utf8'));

    console.log('--- Starting Integration Scenarios ---');

    // 1. 5x5 Matrix Test
    console.log('\n[1] 5x5 Pattern Matrix Test');
    const points = testRoutes.slice(0, 5);
    for (const start of points) {
        for (const end of points) {
            const isSame = start.name === end.name;
            const payload = {
                start_point: { lat: start.lat, lon: start.lon },
                end_point: { lat: end.lat, lon: end.lon }
            };
            const res = await callRouteGenerate(payload);
            console.log(`- ${start.name} -> ${end.name}: Status ${res.status} ${isSame ? '(Same Point)' : ''}`);
        }
    }

    // 2. Waypoints Test
    console.log('\n[2] Waypoints Test');
    const waypointsPayload = {
        start_point: { lat: points[0].lat, lon: points[0].lon },
        end_point: { lat: points[1].lat, lon: points[1].lon },
        waypoints: [
            { lat: points[2].lat, lon: points[2].lon },
            { lat: points[3].lat, lon: points[3].lon }
        ]
    };
    const wpRes = await callRouteGenerate(waypointsPayload);
    console.log(`- With 2 Waypoints: Status ${wpRes.status}`);

    // 3. Distance/Elevation Preferences Test
    console.log('\n[3] Preferences Test (Distance/Elevation)');
    const prefPayload = {
        start_point: { lat: points[0].lat, lon: points[0].lon },
        end_point: { lat: points[1].lat, lon: points[1].lon },
        preferences: {
            target_distance_km: 10.0,
            target_elevation_m: 100.0
        }
    };
    const prefRes = await callRouteGenerate(prefPayload);
    console.log(`- With Preferences: Status ${prefRes.status}`);

    // 4. Boundary Values
    console.log('\n[4] Boundary Values');
    // Very Close
    const closeRes = await callRouteGenerate({
        start_point: { lat: 35.681236, lon: 139.767125 },
        end_point: { lat: 35.681237, lon: 139.767126 }
    });
    console.log(`- Very Close Points: Status ${closeRes.status}`);

    // Very Far (Tokyo to Nagoya)
    const farRes = await callRouteGenerate({
        start_point: { lat: 35.681236, lon: 139.767125 },
        end_point: { lat: 35.170915, lon: 136.881537 }
    });
    console.log(`- Very Far Points: Status ${farRes.status}`);

    // Too Many Waypoints (assuming some limit, testing if it handles gracefully)
    const manyWp = [];
    for(let i=0; i<20; i++) manyWp.push({lat: 35.68 + i*0.001, lon: 139.76 + i*0.001});
    const manyWpRes = await callRouteGenerate({
        start_point: { lat: points[0].lat, lon: points[0].lon },
        end_point: { lat: points[1].lat, lon: points[1].lon },
        waypoints: manyWp
    });
    console.log(`- Many Waypoints (20): Status ${manyWpRes.status}`);

    // 5. Invalid Inputs
    console.log('\n[5] Invalid Inputs & Error Handling');
    for (const test of invalidInputs) {
        const res = await callRouteGenerate(test.payload);
        console.log(`- ${test.description}: Status ${res.status}`);
    }

    console.log('\n--- Integration Scenarios Completed ---');
}

runTests();
