function overview() {
    // Fetch grouped sleep cycle data and display it in chart
    fetch('/upload')  // Adjust the route if needed
        .then(response => response.json())
        .then(data => {
            const ctx = document.getElementById('myChart').getContext('2d');
            const labels = data.map(item => item.start);
            const values = data.map(item => item.duration);
            const sleepStages = data.map(item => item.stage);

            new Chart(ctx, {
                type: 'bar',
                data: {
                    labels: labels,
                    datasets: [{
                        label: 'Sleep Stages',
                        data: values,
                        backgroundColor: sleepStages.map(stage => {
                            if (stage === "Deep Sleep") return 'rgba(54, 162, 235, 0.6)';
                            if (stage === "Light Sleep") return 'rgba(75, 192, 192, 0.6)';
                            if (stage === "REM Sleep") return 'rgba(153, 102, 255, 0.6)';
                            return 'rgba(255, 159, 64, 0.6)';
                        }),
                        borderColor: 'rgba(75, 192, 192, 1)',
                        borderWidth: 1
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
        });
}

function temp() {
    // Fetch temperature data and plot it
    fetchData('/temp');
}

function spo02() {
    // Fetch SpO2 data and plot it
    fetchData('/spo2');
}

function hr() {
    // Fetch heart rate data and plot it
    fetchData('/hr');
}

function fetchData(apiEndpoint) {
    fetch(apiEndpoint)
        .then(response => response.json())
        .then(data => {
            const ctx = document.getElementById('myChart').getContext('2d');
            const labels = data.map(item => item.timestamp);
            const values = data.map(item => item.value);

            new Chart(ctx, {
                type: 'line',
                data: {
                    labels: labels,
                    datasets: [{
                        label: 'Data',
                        data: values,
                        borderColor: 'rgba(75, 192, 192, 1)',
                        fill: false
                    }]
                }
            });
        });
}
