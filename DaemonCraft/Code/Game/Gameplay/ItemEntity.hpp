//----------------------------------------------------------------------------------------------------
// ItemEntity.hpp
//----------------------------------------------------------------------------------------------------
#pragma once

#include "Game/Gameplay/Entity.hpp"
#include "Game/Gameplay/ItemStack.hpp"

//----------------------------------------------------------------------------------------------------
// ItemEntity - Dropped item in the world with physics and magnetic pickup
//
// Represents an item on the ground that can be picked up by players. Follows Minecraft mechanics:
// - Physics: Falls with gravity, collides with blocks
// - Magnetic pickup: Pulls toward player within 3-block radius
// - Item stacking: Merges with nearby identical items (prevents clutter)
// - Despawn: Disappears after 5 minutes to prevent world clutter
//----------------------------------------------------------------------------------------------------
class ItemEntity : public Entity
{
public:
    explicit ItemEntity(Game* owner, Vec3 const& position, ItemStack const& item);
    ~ItemEntity() override = default;

    void Update(float deltaSeconds) override;
    void Render() const override;
    EntityType GetEntityType() const override { return EntityType::ITEM; }

    // Magnetic pickup mechanics
    void ApplyMagneticPull(Vec3 const& playerPosition, float deltaSeconds);
    bool CanBePickedUp() const { return m_pickupCooldown <= 0.0f; }

    // Item stacking (merge with nearby items)
    void TryMergeWithNearbyItems();

    // Pickup handling
    bool TryPickup(class Player* player);  // Returns true if successfully picked up

    // Accessors
    ItemStack const& GetItemStack() const { return m_item; }
    bool IsDespawned() const { return m_despawnTimer <= 0.0f; }

private:
    ItemStack m_item;                  // Item being carried
    float     m_pickupCooldown = 0.5f; // Time after spawn before can pickup (prevents re-pickup)
    float     m_magnetRadius   = 3.0f; // Distance within which item pulls toward player
    float     m_despawnTimer   = 300.0f; // 5 minutes (300 seconds) before despawn
};
