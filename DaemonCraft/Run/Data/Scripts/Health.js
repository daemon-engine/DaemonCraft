// Health.js
// This is a dummy script file for demonstration purposes
// Double-click from the Inspector window will open this file in your default editor

class Health {
    constructor() {
        this.maxHealth = 100;
        this.currentHealth = 85;
        this.regenRate = 1.0;
    }

    update(deltaTime) {
        // Regenerate health over time
        if (this.currentHealth < this.maxHealth) {
            this.currentHealth += this.regenRate * deltaTime;
            this.currentHealth = Math.min(this.currentHealth, this.maxHealth);
        }
    }

    takeDamage(amount) {
        this.currentHealth -= amount;
        if (this.currentHealth <= 0) {
            this.onDeath();
        }
    }

    heal(amount) {
        this.currentHealth = Math.min(this.currentHealth + amount, this.maxHealth);
    }

    onDeath() {
        console.log("Player died!");
    }
}

module.exports = Health;
