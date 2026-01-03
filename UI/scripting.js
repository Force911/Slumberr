let chartInstance = null;

/* ---------- UTILITY ---------- */
function clearChart(ctx) {
    if (chartInstance) {
        chartInstance.destroy();
        chartInstance = null;
    }
}

/* ---------- BASIC DATA FETCH ---------- */
function fetchData(apiEndpoint, label) {
    fetch(apiEndpoint)
        .then(response => response.json())
        .then(data => {
            const ctx = document.getElementById('myChart').getContext('2d');
            clearChart(ctx);

            const labels = data.map(item => item.timestamp);
            const values = data.map(item => item.value);

            chartInstance = new Chart(ctx, {
                type: 'line',
                data: {
                    labels,
                    datasets: [{
                        label,
                        data: values,
                        borderWidth: 2,
                        tension: 0.3
                    }]
                }
            });
        });
}

/* ---------- BUTTON FUNCTIONS ---------- */
function hr() {
    fetchData('/hr', 'Heart Rate (BPM)');
}

function spo02() {
    fetchData('/spo2', 'SpO₂ (%)');
}

function temp() {
    fetchData('/temp', 'Temperature (°C)');
}

/* ---------- SLEEP STAGE DETECTION ---------- */
function detectSleepStage(hr, spo2, temp) {
    if (hr == null || spo2 == null || temp == null) return "Unknown";

    if (hr > 90) return "Awake";

    if (hr < 60 && spo2 > 96 && temp < 36.4) {
        return "Deep Sleep";
    }

    if (hr >= 60 && hr <= 75 && spo2 >= 95) {
        return "Light Sleep";
    }

    if (hr > 75 && spo2 >= 96 && temp >= 36.5) {
        return "REM Sleep";
    }

    return "Light Sleep";
}

/* ---------- SLEEP ANALYSIS ---------- */
function sleepDetection() {
    Promise.all([
        fetch('/hr').then(r => r.json()),
        fetch('/spo2').then(r => r.json()),
        fetch('/temp').then(r => r.json())
    ])
    .then(([hrData, spo2Data, tempData]) => {

        const labels = [];
        const stages = [];

        const len = Math.min(hrData.length, spo2Data.length, tempData.length);

        for (let i = 0; i < len; i++) {
            const stage = detectSleepStage(
                hrData[i].value,
                spo2Data[i].value,
                tempData[i].value
            );

            labels.push(hrData[i].timestamp);
            stages.push(stage);
        }

        plotSleepStages(labels, stages);
    });
}

/* ---------- PLOT SLEEP STAGES ---------- */
function plotSleepStages(labels, stages) {
    const ctx = document.getElementById('myChart').getContext('2d');
    clearChart(ctx);

    const stageMap = {
        "Deep Sleep": 1,
        "Light Sleep": 2,
        "REM Sleep": 3,
        "Awake": 4
    };

    const colors = {
        "Deep Sleep": "rgba(54, 162, 235, 0.7)",
        "Light Sleep": "rgba(75, 192, 192, 0.7)",
        "REM Sleep": "rgba(153, 102, 255, 0.7)",
        "Awake": "rgba(255, 99, 132, 0.7)"
    };

    chartInstance = new Chart(ctx, {
        type: 'bar',
        data: {
            labels,
            datasets: [{
                label: 'Sleep Stage',
                data: stages.map(s => stageMap[s]),
                backgroundColor: stages.map(s => colors[s])
            }]
        },
        options: {
            responsive: true,
            scales: {
                y: {
                    min: 0,
                    max: 4,
                    ticks: {
                        stepSize: 1,
                        callback: value => {
                            return Object.keys(stageMap).find(k => stageMap[k] === value);
                        }
                    },
                    title: {
                        display: true,
                        text: 'Sleep Stage'
                    }
                },
                x: {
                    title: {
                        display: true,
                        text: 'Time'
                    }
                }
            }
        }
    });
}

/* ---------- OVERVIEW (SERVER GROUPED DATA) ---------- */
function overview() {
    fetch('/upload')
        .then(response => response.json())
        .then(data => {
            const ctx = document.getElementById('myChart').getContext('2d');
            clearChart(ctx);

            const labels = data.map(d => d.start);
            const values = data.map(d => d.duration);
            const stages = data.map(d => d.stage);

            const colors = {
                "Deep Sleep": "rgba(54, 162, 235, 0.6)",
                "Light Sleep": "rgba(75, 192, 192, 0.6)",
                "REM Sleep": "rgba(153, 102, 255, 0.6)",
                "Awake": "rgba(255, 159, 64, 0.6)"
            };

            chartInstance = new Chart(ctx, {
                type: 'bar',
                data: {
                    labels,
                    datasets: [{
                        label: 'Sleep Cycles',
                        data: values,
                        backgroundColor: stages.map(s => colors[s] || colors["Awake"])
                    }]
                },
                options: {
                    scales: {
                        y: {
                            beginAtZero: true,
                            title: {
                                display: true,
                                text: 'Duration (seconds)'
                            }
                        }
                    }
                }
            });
        });
}
