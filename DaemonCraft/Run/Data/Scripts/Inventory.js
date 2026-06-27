// Inventory.js
// This is a dummy script file for demonstration purposes
// Double-click from the Inspector window will open this file in your default editor

class Inventory {
    constructor() {
        this.capacity = 20;
        this.items = [];
        this.autoSort = true;
    }

    addItem(item) {
        if (this.items.length < this.capacity) {
            this.items.push(item);
            if (this.autoSort) {
                this.sortItems();
            }
            return true;
        }
        return false; // Inventory full
    }

    removeItem(itemId) {
        const index = this.items.findIndex(item => item.id === itemId);
        if (index !== -1) {
            this.items.splice(index, 1);
            return true;
        }
        return false;
    }

    sortItems() {
        this.items.sort((a, b) => a.name.localeCompare(b.name));
    }

    clearInventory() {
        this.items = [];
        console.log("Inventory cleared");
    }

    getItemCount() {
        return this.items.length;
    }
}

module.exports = Inventory;
