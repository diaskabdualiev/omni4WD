/**
 * Web Bluetooth API wrapper for Omni Robot
 * Handles BLE connection and communication with ESP32
 */

class RobotBluetooth {
  constructor() {
    this.device = null;
    this.server = null;
    this.service = null;
    this.characteristics = {};

    // BLE UUIDs (must match ESP32 firmware)
    this.SERVICE_UUID = '4fafc201-1fb5-459e-8fcc-c5c9c331914b';
    this.CHAR_UUIDS = {
      command:  'beb5483e-36e1-4688-b7f5-ea07361b26a8',
      joystick: 'ca73b3ba-39f6-4ab3-91ae-186dc9577d99',
      speed:    '1c95d5e3-d8f7-413a-bf3d-7a2e5d7be87e',
      config:   'd4e1f1a2-8b5c-4d3e-9f7a-6c8b5a4d3e2f',
      test:     'a3b2c1d4-5e6f-7a8b-9c0d-1e2f3a4b5c6d'
    };

    // Callbacks
    this.onConnected = null;
    this.onDisconnected = null;
    this.onConfigReceived = null;
  }

  /**
   * Connect to ESP32 BLE device
   */
  async connect() {
    try {
      console.log('[BLE] Requesting device...');

      // Request device from user
      this.device = await navigator.bluetooth.requestDevice({
        filters: [{ name: 'Omni Robot' }],
        optionalServices: [this.SERVICE_UUID]
      });

      console.log('[BLE] Device selected:', this.device.name);

      // Listen for disconnection
      this.device.addEventListener('gattserverdisconnected', () => {
        console.log('[BLE] Device disconnected');
        if (this.onDisconnected) {
          this.onDisconnected();
        }
      });

      // Connect to GATT server
      console.log('[BLE] Connecting to GATT server...');
      this.server = await this.device.gatt.connect();

      // Get service
      console.log('[BLE] Getting service...');
      this.service = await this.server.getPrimaryService(this.SERVICE_UUID);

      // Get all characteristics
      console.log('[BLE] Getting characteristics...');
      for (const [name, uuid] of Object.entries(this.CHAR_UUIDS)) {
        this.characteristics[name] = await this.service.getCharacteristic(uuid);
        console.log(`[BLE] ✓ Characteristic '${name}' obtained`);
      }

      // Subscribe to config notifications
      await this.characteristics.config.startNotifications();
      this.characteristics.config.addEventListener('characteristicvaluechanged', (event) => {
        const value = new TextDecoder().decode(event.target.value);
        console.log('[BLE] Config received:', value);
        try {
          const config = JSON.parse(value);
          if (this.onConfigReceived) {
            this.onConfigReceived(config);
          }
        } catch (e) {
          console.error('[BLE] Failed to parse config:', e);
        }
      });

      console.log('[BLE] ✓ Connection successful!');

      if (this.onConnected) {
        this.onConnected();
      }

      // Request current config
      await this.getConfig();

    } catch (error) {
      console.error('[BLE] Connection error:', error);
      throw error;
    }
  }

  /**
   * Disconnect from device
   */
  async disconnect() {
    if (this.device && this.device.gatt.connected) {
      await this.device.gatt.disconnect();
      console.log('[BLE] Disconnected');
    }
  }

  /**
   * Send movement command
   * @param {string} command - forward, backward, left, right, rotate_left, rotate_right, stop
   */
  async sendCommand(command) {
    if (!this.characteristics.command) {
      console.warn('[BLE] Command characteristic not available');
      return;
    }

    try {
      const encoder = new TextEncoder();
      await this.characteristics.command.writeValue(encoder.encode(command));
      console.log('[BLE] Command sent:', command);
    } catch (error) {
      console.error('[BLE] Failed to send command:', error);
    }
  }

  /**
   * Send joystick position
   * @param {number} x - X coordinate (-255 to 255)
   * @param {number} y - Y coordinate (-255 to 255)
   */
  async sendJoystick(x, y) {
    if (!this.characteristics.joystick) {
      console.warn('[BLE] Joystick characteristic not available');
      return;
    }

    try {
      // Convert -255..255 to -128..127 (int8_t range)
      const xByte = Math.round(Math.max(-128, Math.min(127, x * 127 / 255)));
      const yByte = Math.round(Math.max(-128, Math.min(127, y * 127 / 255)));

      const data = new Int8Array([xByte, yByte]);
      await this.characteristics.joystick.writeValue(data);

      // Uncomment for debugging
      // console.log(`[BLE] Joystick: x=${xByte}, y=${yByte}`);
    } catch (error) {
      console.error('[BLE] Failed to send joystick data:', error);
    }
  }

  /**
   * Set motor speed
   * @param {number} speed - Speed value (0-255)
   */
  async setSpeed(speed) {
    if (!this.characteristics.speed) {
      console.warn('[BLE] Speed characteristic not available');
      return;
    }

    try {
      const data = new Uint8Array([Math.max(0, Math.min(255, speed))]);
      await this.characteristics.speed.writeValue(data);
      console.log('[BLE] Speed set to:', speed);
    } catch (error) {
      console.error('[BLE] Failed to set speed:', error);
    }
  }

  /**
   * Get current config from ESP32
   * @returns {Promise<Object>} Config object with mapping and invert arrays
   */
  async getConfig() {
    if (!this.characteristics.config) {
      console.warn('[BLE] Config characteristic not available');
      return null;
    }

    try {
      const value = await this.characteristics.config.readValue();
      const json = new TextDecoder().decode(value);
      const config = JSON.parse(json);
      console.log('[BLE] Config read:', config);
      return config;
    } catch (error) {
      console.error('[BLE] Failed to read config:', error);
      return null;
    }
  }

  /**
   * Send test motor command
   * @param {string} command - test_0_fwd, test_1_bwd, etc.
   */
  async sendTestCommand(command) {
    if (!this.characteristics.test) {
      console.warn('[BLE] Test characteristic not available');
      return;
    }

    try {
      const encoder = new TextEncoder();
      await this.characteristics.test.writeValue(encoder.encode(command));
      console.log('[BLE] Test command sent:', command);
    } catch (error) {
      console.error('[BLE] Failed to send test command:', error);
    }
  }

  /**
   * Send config command (set_map, set_inv, save_config)
   * @param {string} command - Configuration command string
   */
  async sendConfigCommand(command) {
    if (!this.characteristics.command) {
      console.warn('[BLE] Command characteristic not available');
      return;
    }

    try {
      const encoder = new TextEncoder();
      await this.characteristics.command.writeValue(encoder.encode(command));
      console.log('[BLE] Config command sent:', command);
    } catch (error) {
      console.error('[BLE] Failed to send config command:', error);
    }
  }

  /**
   * Check if device is connected
   * @returns {boolean} True if connected
   */
  isConnected() {
    return this.device && this.device.gatt.connected;
  }
}
