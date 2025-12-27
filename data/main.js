let isAutoMode = false;

function switchTab(tabName) {
    // Hide all tabs
    document.querySelectorAll('.tab-content').forEach(tab => {
        tab.classList.remove('active');
    });
    document.querySelectorAll('.nav-tab').forEach(tab => {
        tab.classList.remove('active');
    });

    // Show selected tab
    document.getElementById(tabName).classList.add('active');
    event.target.classList.add('active');

    if (tabName === 'settings') {
        loadSettings();
    }
}

// Load current settings
function loadSettings() {
    fetch('/api/config')
        .then(res => res.json())
        .then(data => {
            document.getElementById('dryValue').value = data.dryValue;
            document.getElementById('wetValue').value = data.wetValue;
            document.getElementById('threshold').value = data.dryThreshold;
        })
        .catch(err => console.log('Error loading settings:', err));
}

// Save settings
function saveSettings() {
    const dryValue = parseInt(document.getElementById('dryValue').value);
    const wetValue = parseInt(document.getElementById('wetValue').value);
    const threshold = parseInt(document.getElementById('threshold').value);

    fetch('/api/config', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({
            dryValue: dryValue,
            wetValue: wetValue,
            dryThreshold: threshold
        })
    })
    .then(res => res.json())
    .then(data => {
        const msg = document.getElementById('successMsg');
        msg.classList.add('show');
        setTimeout(() => msg.classList.remove('show'), 3000);
    })
    .catch(err => console.log('Error saving settings:', err));
}

// Update sensor readings every 2 seconds
setInterval(() => {
    fetch('/api/sensors')
        .then(res => res.json())
        .then(data => {
            document.getElementById('moisture').textContent = Math.round(data.moisture);

            if (isAutoMode && data.irrigation !== undefined) {
                updateIrrigationButtonState(data.irrigation);
            }
        })
        .catch(err => console.log('Error fetching sensors:', err));
}, 2000);

function toggleMode(button) {
    button.classList.toggle('active');
    isAutoMode = button.classList.contains('active');
    modeStatus = document.getElementById('modeStatus');

    const modeInfo = document.getElementById('modeInfo');
    const irrigationBtn = document.getElementById('irrigationBtn');

    if (isAutoMode) {
        irrigationBtn.classList.add('disabled');
        modeStatus.innerHTML = 'ON'
    } else {
        irrigationBtn.classList.remove('disabled');
        modeStatus.innerHTML = 'OFF'
    }

    fetch('/api/control', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({device: 'auto', state: isAutoMode})
    }).catch(err => console.log('Error:', err));
}

function toggleIrrigation(button) {
    if (isAutoMode) return;

    button.classList.toggle('active');
    const status = button.querySelector('.control-status');
    const isActive = button.classList.contains('active');
    status.textContent = isActive ? 'ON' : 'OFF';

    fetch('/api/control', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({device: 'irrigation', state: isActive})
    }).catch(err => console.log('Error:', err));
}

function updateIrrigationButtonState(isActive) {
    const irrigationBtn = document.getElementById('irrigationBtn');
    const status = irrigationBtn.querySelector('.control-status');

    if (isActive) {
        irrigationBtn.classList.add('active');
        status.textContent = 'ON';
    } else {
        irrigationBtn.classList.remove('active');
        status.textContent = 'OFF';
    }
}