/**
 * Canvas-based Joystick controller
 * Handles touch and mouse events for joystick control
 */

class Joystick {
  constructor(canvasId) {
    this.canvas = document.getElementById(canvasId);
    if (!this.canvas) {
      console.error('[Joystick] Canvas not found:', canvasId);
      return;
    }

    this.ctx = this.canvas.getContext('2d');
    this.active = false;
    this.x = 0;  // Current X position (-255 to 255)
    this.y = 0;  // Current Y position (-255 to 255)

    // Callbacks
    this.onMove = null;   // callback(x, y)
    this.onStop = null;   // callback()

    this.init();
  }

  init() {
    // Set canvas size to match element size
    const rect = this.canvas.getBoundingClientRect();
    this.canvas.width = rect.width;
    this.canvas.height = rect.height;

    this.centerX = this.canvas.width / 2;
    this.centerY = this.canvas.height / 2;
    this.maxRadius = Math.min(this.canvas.width, this.canvas.height) / 2 - 20;

    // Touch events
    this.canvas.addEventListener('touchstart', (e) => this.handleTouchStart(e), { passive: false });
    this.canvas.addEventListener('touchmove', (e) => this.handleTouchMove(e), { passive: false });
    this.canvas.addEventListener('touchend', (e) => this.handleTouchEnd(e), { passive: false });

    // Mouse events
    this.canvas.addEventListener('mousedown', (e) => this.handleMouseDown(e));
    this.canvas.addEventListener('mousemove', (e) => this.handleMouseMove(e));
    this.canvas.addEventListener('mouseup', () => this.handleEnd());
    this.canvas.addEventListener('mouseleave', () => this.handleEnd());

    this.draw();
  }

  draw() {
    this.ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);

    // Draw outer circle
    this.ctx.beginPath();
    this.ctx.arc(this.centerX, this.centerY, this.maxRadius, 0, 2 * Math.PI);
    this.ctx.strokeStyle = '#e2e8f0';
    this.ctx.lineWidth = 2;
    this.ctx.stroke();

    // Draw center dot
    this.ctx.beginPath();
    this.ctx.arc(this.centerX, this.centerY, 5, 0, 2 * Math.PI);
    this.ctx.fillStyle = '#cbd5e1';
    this.ctx.fill();

    // Draw stick
    const stickX = this.centerX + (this.x * this.maxRadius / 255);
    const stickY = this.centerY + (this.y * this.maxRadius / 255);

    this.ctx.beginPath();
    this.ctx.arc(stickX, stickY, 30, 0, 2 * Math.PI);
    this.ctx.fillStyle = this.active ? '#3b82f6' : '#94a3b8';
    this.ctx.fill();
    this.ctx.strokeStyle = 'white';
    this.ctx.lineWidth = 3;
    this.ctx.stroke();
  }

  calculatePosition(clientX, clientY) {
    const rect = this.canvas.getBoundingClientRect();
    const x = clientX - rect.left - this.centerX;
    const y = clientY - rect.top - this.centerY;

    const distance = Math.sqrt(x * x + y * y);
    const angle = Math.atan2(y, x);

    // Clamp distance to max radius
    const clampedDistance = Math.min(distance, this.maxRadius);

    // Convert to -255..255 range
    this.x = Math.round((clampedDistance * Math.cos(angle) / this.maxRadius) * 255);
    this.y = -Math.round((clampedDistance * Math.sin(angle) / this.maxRadius) * 255);  // Invert Y for natural control

    this.draw();

    if (this.onMove) {
      this.onMove(this.x, this.y);
    }
  }

  handleTouchStart(e) {
    e.preventDefault();
    this.active = true;
    this.calculatePosition(e.touches[0].clientX, e.touches[0].clientY);
  }

  handleTouchMove(e) {
    e.preventDefault();
    if (this.active) {
      this.calculatePosition(e.touches[0].clientX, e.touches[0].clientY);
    }
  }

  handleTouchEnd(e) {
    e.preventDefault();
    this.handleEnd();
  }

  handleMouseDown(e) {
    this.active = true;
    this.calculatePosition(e.clientX, e.clientY);
  }

  handleMouseMove(e) {
    if (this.active) {
      this.calculatePosition(e.clientX, e.clientY);
    }
  }

  handleEnd() {
    this.active = false;
    this.x = 0;
    this.y = 0;
    this.draw();

    if (this.onStop) {
      this.onStop();
    }
  }
}
