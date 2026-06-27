//----------------------------------------------------------------------------------------------------
// Registry.hpp
// Generic registry template for Assignment 7 (A7-Core)
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#pragma once
#include <algorithm>
#include <map>
#include <shared_mutex>
#include <string>
#include <vector>

//----------------------------------------------------------------------------------------------------
/// @brief Generic registry template for managing game definitions (blocks, items, recipes)
/// @details Thread-safe registry with ID-based and name-based lookups. Uses sequential ID assignment
///          where ID = index in vector. Supports concurrent reads via std::shared_mutex.
/// @tparam T Type of object to register (e.g., BlockDefinition, ItemDefinition, RecipeDefinition)
///
/// Example Usage:
///   Registry<ItemDefinition> itemRegistry;
///   itemRegistry.Register("Diamond_Pickaxe", new ItemDefinition(...));
///   ItemDefinition* def = itemRegistry.Get("diamond_pickaxe"); // Case-insensitive
///   ItemDefinition* def2 = itemRegistry.Get(42); // By ID
///
template <typename T>
class Registry
{
public:
	Registry() = default;
	~Registry();

	// Registration
	void Register(std::string const& name, T* object);

	// Lookup by ID (O(1) vector access)
	T* Get(uint16_t id) const;

	// Lookup by name (O(log n) map access, case-insensitive)
	T*       Get(std::string const& name) const;
	uint16_t GetID(std::string const& name) const;

	// Iteration and inspection
	size_t                      GetCount() const;
	std::vector<T*> const&      GetAll() const;

private:
	std::vector<T*>              m_objects;   // ID → Object (index = ID)
	std::map<std::string, uint16_t> m_nameToID;  // Lowercase name → ID
	mutable std::shared_mutex    m_mutex;     // Thread-safe concurrent reads

	// Helper: Convert name to lowercase for case-insensitive lookups
	static std::string ToLowercase(std::string const& str);
};

//----------------------------------------------------------------------------------------------------
// Implementation (Header-only template)
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
template <typename T>
Registry<T>::~Registry()
{
	// Clean up all registered objects
	std::unique_lock lock(m_mutex);
	for (T* object : m_objects)
	{
		delete object;
	}
	m_objects.clear();
	m_nameToID.clear();
}

//----------------------------------------------------------------------------------------------------
template <typename T>
void Registry<T>::Register(std::string const& name, T* object)
{
	std::unique_lock lock(m_mutex);

	// Sequential ID assignment: ID = current vector size (next available index)
	uint16_t id = static_cast<uint16_t>(m_objects.size());

	// Store object in vector (ID → Object via index)
	m_objects.push_back(object);

	// Store name → ID mapping (case-insensitive)
	std::string lowercaseName = ToLowercase(name);
	m_nameToID[lowercaseName] = id;
}

//----------------------------------------------------------------------------------------------------
template <typename T>
T* Registry<T>::Get(uint16_t id) const
{
	std::shared_lock lock(m_mutex);

	// Bounds check
	if (id >= m_objects.size())
	{
		return nullptr;
	}

	return m_objects[id];
}

//----------------------------------------------------------------------------------------------------
template <typename T>
T* Registry<T>::Get(std::string const& name) const
{
	uint16_t id = GetID(name);
	if (id == static_cast<uint16_t>(-1))
	{
		return nullptr;
	}
	return Get(id);
}

//----------------------------------------------------------------------------------------------------
template <typename T>
uint16_t Registry<T>::GetID(std::string const& name) const
{
	std::shared_lock lock(m_mutex);

	std::string lowercaseName = ToLowercase(name);
	auto it = m_nameToID.find(lowercaseName);

	if (it == m_nameToID.end())
	{
		return static_cast<uint16_t>(-1); // Invalid ID (0xFFFF)
	}

	return it->second;
}

//----------------------------------------------------------------------------------------------------
template <typename T>
size_t Registry<T>::GetCount() const
{
	std::shared_lock lock(m_mutex);
	return m_objects.size();
}

//----------------------------------------------------------------------------------------------------
template <typename T>
std::vector<T*> const& Registry<T>::GetAll() const
{
	// Caller must ensure thread safety when iterating
	// Recommendation: Copy vector if iteration is needed outside lock scope
	return m_objects;
}

//----------------------------------------------------------------------------------------------------
template <typename T>
std::string Registry<T>::ToLowercase(std::string const& str)
{
	std::string result = str;
	std::transform(result.begin(), result.end(), result.begin(),
		[](unsigned char c) { return static_cast<unsigned char>(std::tolower(c)); });
	return result;
}
