// PlayerController.js
// This is a dummy script file for demonstration purposes
// Double-click from the Inspector window will open this file in your default editor

class PlayerController {
    constructor() {
        this.moveSpeed = 5.5;
        this.jumpHeight = 2.0;
        this.canDoubleJump = true;
    }

    update(deltaTime) {
        // Player movement logic here
        console.log("PlayerController update");
    }

    onJump() {
        console.log("Player jumped!");
    }

    onMove(direction) {
        console.log("Player moved in direction:", direction);
    }
}

module.exports = PlayerController;
