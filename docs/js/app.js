/**
 * Main application logic
 * Connects UI with Bluetooth and Joystick components
 */

let robot = null;
let joystick = null;

const statusEl = document.getElementById('status');
const connectBtn = document.getElementById('connectBtn');

// ========== Initialization ==========

function init() {
  console.log('[App] Initializing...');

  // Create robot instance
  robot = new RobotBluetooth();

  // Set up callbacks
  robot.onConnected = () => {
    console.log('[App] Robot connected');
    statusEl.textContent = '✓ Подключено';
    statusEl.className = 'status connected';
    connectBtn.textContent = '✓ Подключено';
    connectBtn.classList.add('connected');
    connectBtn.disabled = true;
  };

  robot.onDisconnected = () => {
    console.log('[App] Robot disconnected');
    statusEl.textContent = '✗ Отключено';
    statusEl.className = 'status disconnected';
    connectBtn.textContent = '🔗 Подключить робота';
    connectBtn.classList.remove('connected');
    connectBtn.disabled = false;
  };

  robot.onConfigReceived = (config) => {
    console.log('[App] Config received:', config);
    loadConfigToUI(config);
  };

  // Connect button handler
  connectBtn.addEventListener('click', async () => {
    try {
      connectBtn.disabled = true;
      connectBtn.textContent = 'Подключение...';
      await robot.connect();
    } catch (error) {
      console.error('[App] Connection failed:', error);
      alert('Не удалось подключиться к роботу:\n' + error.message);
      connectBtn.disabled = false;
      connectBtn.textContent = '🔗 Подключить робота';
    }
  });

  // Initialize joystick
  setTimeout(() => {
    initJoystick();
  }, 500);

  console.log('[App] Initialization complete');
}

// ========== Joystick ==========

function initJoystick() {
  joystick = new Joystick('joystickCanvas');

  joystick.onMove = (x, y) => {
    if (robot && robot.isConnected()) {
      robot.sendJoystick(x, y);
    }
  };

  joystick.onStop = () => {
    if (robot && robot.isConnected()) {
      robot.sendCommand('stop');
    }
  };

  console.log('[App] Joystick initialized');
}

// ========== Commands ==========

function sendCommand(cmd) {
  if (robot && robot.isConnected()) {
    robot.sendCommand(cmd);
  } else {
    console.warn('[App] Robot not connected');
  }
}

function sendTestCommand(cmd) {
  if (robot && robot.isConnected()) {
    robot.sendTestCommand(cmd);
  } else {
    console.warn('[App] Robot not connected');
  }
}

// ========== Speed Control ==========

function updateSpeed() {
  const speed = document.getElementById('speedSlider').value;
  document.getElementById('speedValue').textContent = speed;
  document.getElementById('speedSlider2').value = speed;
  document.getElementById('speedValue2').textContent = speed;

  if (robot && robot.isConnected()) {
    robot.setSpeed(parseInt(speed));
  }
}

function updateSpeed2() {
  const speed = document.getElementById('speedSlider2').value;
  document.getElementById('speedValue2').textContent = speed;
  document.getElementById('speedSlider').value = speed;
  document.getElementById('speedValue').textContent = speed;

  if (robot && robot.isConnected()) {
    robot.setSpeed(parseInt(speed));
  }
}

// ========== Tab Switching ==========

function switchTab(index) {
  const tabs = document.querySelectorAll('.tab');
  const contents = document.querySelectorAll('.tab-content');

  tabs.forEach((tab, i) => {
    tab.classList.toggle('active', i === index);
  });

  contents.forEach((content, i) => {
    content.classList.toggle('active', i === index);
  });

  // Stop motors when switching tabs
  sendCommand('stop');
}

// ========== Mode Switching ==========

function switchMode(mode) {
  const joystickMode = document.getElementById('joystick-mode');
  const buttonsMode = document.getElementById('buttons-mode');
  const btnJoystick = document.getElementById('modeJoystick');
  const btnButtons = document.getElementById('modeButtons');

  if (mode === 'joystick') {
    joystickMode.style.display = 'block';
    buttonsMode.style.display = 'none';
    btnJoystick.classList.add('active');
    btnButtons.classList.remove('active');

    // Reinitialize joystick
    setTimeout(initJoystick, 100);
  } else {
    joystickMode.style.display = 'none';
    buttonsMode.style.display = 'block';
    btnJoystick.classList.remove('active');
    btnButtons.classList.add('active');
  }

  // Stop motors when switching modes
  sendCommand('stop');
}

// ========== Calibration ==========

function loadConfigToUI(config) {
  console.log('[App] Loading config to UI:', config);

  for (let i = 0; i < 4; i++) {
    document.getElementById('map' + i).value = config.mapping[i];
    document.getElementById('inv' + i).checked = config.invert[i];
  }
}

function updateMapping(pos) {
  const value = document.getElementById('map' + pos).value;
  const command = `set_map:${pos}:${value}`;
  console.log('[App] Sending mapping command:', command);

  if (robot && robot.isConnected()) {
    robot.sendConfigCommand(command);
  }
}

function updateInvert(pos) {
  const value = document.getElementById('inv' + pos).checked;
  const command = `set_inv:${pos}:${value}`;
  console.log('[App] Sending invert command:', command);

  if (robot && robot.isConnected()) {
    robot.sendConfigCommand(command);
  }
}

function saveSettings() {
  console.log('[App] Saving settings...');

  if (!robot || !robot.isConnected()) {
    alert('Робот не подключен!');
    return;
  }

  // Apply all current settings
  for (let i = 0; i < 4; i++) {
    updateMapping(i);
    updateInvert(i);
  }

  // Wait a bit for commands to be sent
  setTimeout(() => {
    robot.sendConfigCommand('save_config');
    alert('💾 Настройки сохранены в память ESP32!');
  }, 500);
}

function resetSettings() {
  if (!robot || !robot.isConnected()) {
    alert('Робот не подключен!');
    return;
  }

  if (confirm('Сбросить все настройки к дефолту?')) {
    // Reset UI
    for (let i = 0; i < 4; i++) {
      document.getElementById('map' + i).value = i + 1;
      document.getElementById('inv' + i).checked = false;
    }
    alert('🔄 Настройки сброшены в интерфейсе!\nНе забудь нажать "Сохранить" чтобы применить.');
  }
}

// ========== Prevent text selection ==========

document.addEventListener('selectstart', function(e) {
  e.preventDefault();
});

// ========== Handle orientation change ==========

window.addEventListener('orientationchange', () => {
  setTimeout(() => {
    if (joystick) {
      console.log('[App] Orientation changed, reinitializing joystick');
      initJoystick();
    }
  }, 200);
});

// Also handle resize for joystick recalculation
window.addEventListener('resize', () => {
  if (joystick) {
    setTimeout(() => {
      initJoystick();
    }, 100);
  }
});

// ========== Initialize on load ==========

if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', init);
} else {
  init();
}
