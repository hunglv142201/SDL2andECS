#pragma once

#include <algorithm>
#include <array>
#include <bitset>
#include <exception>
#include <queue>
#include <stdexcept>
#include <unordered_map>
#include <vector>

const int MAX_ENTITIES = 512;
const int MAX_COMPONENTS = 2;

using Entity = int;
using Signature = std::bitset<MAX_COMPONENTS>;

class ECS;

class ECSException : public std::exception
{
  public:
    ECSException(const char* message) { this->message = message; }

    const char* what() const throw() { return message; }

  private:
    const char* message;
};

class ECSUtils
{
  public:
    template <typename T>
    static void swapRemove(std::vector<T>& vec, int index)
    {
        if (vec.size() == 0)
        {
            return;
        }

        if (index < 0 || index > vec.size() - 1)
        {
            return;
        }

        if (vec.size() > 1)
        {
            std::iter_swap(vec.begin() + index, vec.end() - 1);
        }
        vec.pop_back();
    }
};

class EntityManager
{
  public:
    EntityManager()
    {
        for (int i = 0; i < MAX_ENTITIES; i++)
        {
            availableEntities.push(i);
        }
    }

    Entity createNew()
    {
        if (availableEntities.empty())
        {
            throw ECSException("Out of available entities");
        }

        Entity entity = availableEntities.front();
        availableEntities.pop();
        return entity;
    }

    void destroy(Entity entity)
    {
        availableEntities.push(entity);
        signatures[entity].reset();
    }

    Signature& getSignature(Entity entity) { return signatures[entity]; }

    void setSignature(Entity entity, Signature& signature) { signatures[entity] = signature; }

  private:
    std::queue<Entity> availableEntities;
    std::array<Signature, MAX_ENTITIES> signatures;
};

class IComponentPool
{
  public:
    virtual ~IComponentPool() = default;
    virtual void onEntityDestroy(Entity entity) = 0;
};

template <typename T>
class ComponentPool : public IComponentPool
{
  public:
    void add(Entity entity, T& component)
    {
        components.push_back(component);
        entityToIndex[entity] = components.size() - 1;
        indexToEntity[components.size() - 1] = entity;
    }

    void remove(Entity removeEntity)
    {
        int indexOfRemoveEntity = entityToIndex[removeEntity];
        int indexOfLastComponent = components.size() - 1;
        Entity entityOfLastComponent = indexToEntity[indexOfLastComponent];

        ECSUtils::swapRemove(components, indexOfRemoveEntity);

        entityToIndex[entityOfLastComponent] = indexOfRemoveEntity;
        indexToEntity[indexOfRemoveEntity] = entityOfLastComponent;

        entityToIndex.erase(removeEntity);
        indexToEntity.erase(indexOfRemoveEntity);
    }

    T* get(Entity entity)
    {
        try
        {
            return &(components[entityToIndex[entity]]);
        }
        catch (std::out_of_range e)
        {
            return nullptr;
        }
    }

    void onEntityDestroy(Entity entity) override { remove(entity); }

  private:
    std::vector<T> components;
    std::unordered_map<Entity, int> entityToIndex;
    std::unordered_map<int, Entity> indexToEntity;

    // template <typename T1, typename T2>
    // bool isOutOfRange(std::unordered_map<T1, T2>& map, T1& index)
    // {
    //     return map.find(index) == map.end();
    // }
};

class ComponentManager
{
  public:
    ComponentManager()
    {
        for (IComponentPool*& componentPool : componentPoolArray)
        {
            componentPool = nullptr;
        }
    }

    ~ComponentManager()
    {
        for (IComponentPool* componentPool : componentPoolArray)
        {
            if (componentPool != nullptr)
            {
                delete componentPool;
            }
        }
    }

    template <typename T>
    void registerComponent()
    {
        int id = getComponentTypeId<T>();
        componentPoolArray[id] = new ComponentPool<T>();
    }

    template <typename T>
    void add(Entity entity, T& component)
    {
        ComponentPool<T>* componentPool = getComponentPool<T>();
        if (componentPool == nullptr)
        {
            return;
        }

        componentPool->add(entity, component);
    }

    template <typename T>
    void remove(Entity entity)
    {
        ComponentPool<T>* componentPool = getComponentPool<T>();
        if (componentPool == nullptr)
        {
            return;
        }

        componentPool->remove(entity);
    }

    template <typename T>
    T* get(Entity entity)
    {
        ComponentPool<T>* componentPool = getComponentPool<T>();

        return componentPool->get(entity);
    }

    template <typename T>
    static int getComponentTypeId()
    {
        static int id = getComponentTypeId();
        return id;
    }

    void onEntityDestroy(Entity entity)
    {
        for (IComponentPool* componentPool : componentPoolArray)
        {
            if (componentPool != nullptr)
            {
                componentPool->onEntityDestroy(entity);
            }
        }
    }

  private:
    std::array<IComponentPool*, MAX_COMPONENTS> componentPoolArray;

    template <typename T>
    ComponentPool<T>* getComponentPool()
    {
        int id = getComponentTypeId<T>();
        IComponentPool* ptr = componentPoolArray[id];

        return dynamic_cast<ComponentPool<T>*>(ptr);
    }

    static int getComponentTypeId()
    {
        static int id = 0;
        return id++;
    }
};

class ISystem
{
  public:
    virtual ~ISystem() = default;
    virtual void process(const float deltaTime, std::vector<Entity>& entities, ECS& ecs) = 0;
    virtual Signature getSignature() = 0;

  protected:
    template <typename T>
    int getComponentTypeId()
    {
        return ComponentManager::getComponentTypeId<T>();
    }
};

class SystemManager
{
  public:
    ~SystemManager()
    {
        for (ISystem* system : systems)
        {
            if (system != nullptr)
            {
                delete system;
            }
        }
    }

    template <typename T>
    void registerSystem(T* system)
    {
        systems.push_back(system);
        entitiesVec.push_back(std::vector<Entity>());
    }

    void onEntitySignatureChange(Entity entity, Signature& signature)
    {
        for (int i = 0; i < systems.size(); i++)
        {
            Signature systemSignature = systems[i]->getSignature();
            if ((signature & systemSignature) == systemSignature)
            {
                if (std::find(entitiesVec[i].begin(), entitiesVec[i].end(), entity) == entitiesVec[i].end())
                {
                    entitiesVec[i].push_back(entity);
                }
            }
            else
            {
                ECSUtils::swapRemove(entitiesVec[i], entity);
            }
        }
    }

    void onEntityDestroy(Entity entity)
    {
        for (int i = 0; i < systems.size(); i++)
        {
            ECSUtils::swapRemove(entitiesVec[i], entity);
        }
    }

    void process(float deltaTime, ECS& ecs)
    {
        for (int i = 0; i < systems.size(); i++)
        {
            systems[i]->process(deltaTime, entitiesVec[i], ecs);
        }
    }

  private:
    std::vector<ISystem*> systems;
    std::vector<std::vector<Entity>> entitiesVec;
};

class ECS
{
  public:
    Entity newEntity() { return entityManager.createNew(); }

    Signature getSignature(Entity entity) { return entityManager.getSignature(entity); }

    void destroyEntity(Entity entity)
    {
        entityManager.destroy(entity);
        componentManager.onEntityDestroy(entity);
        systemManager.onEntityDestroy(entity);
    }

    template <typename T>
    void registerComponent()
    {
        componentManager.registerComponent<T>();
    }

    template <typename T>
    void assignComponent(Entity entity, T& component)
    {
        componentManager.add(entity, component);

        Signature signature = entityManager.getSignature(entity);
        signature.set(componentManager.getComponentTypeId<T>());
        entityManager.setSignature(entity, signature);

        systemManager.onEntitySignatureChange(entity, signature);
    }

    template <typename T>
    void removeComponent(Entity entity)
    {
        componentManager.remove<T>(entity);

        Signature signature = entityManager.getSignature(entity);
        signature.reset(componentManager.getComponentTypeId<T>());
        entityManager.setSignature(entity, signature);

        systemManager.onEntitySignatureChange(entity, signature);
    }

    template <typename T>
    T* getComponent(Entity entity)
    {
        return componentManager.get<T>(entity);
    }

    template <typename T>
    void registerSystem(T* system)
    {
        systemManager.registerSystem(system);
    }

    void processSystem(float deltaTime = 1.0f) { systemManager.process(deltaTime, *this); }

  private:
    EntityManager entityManager;
    ComponentManager componentManager;
    SystemManager systemManager;
};
