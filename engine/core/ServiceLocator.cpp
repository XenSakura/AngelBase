module;
#include <cassert>

export module ServiceLocator;
import std;
// all systems should inherit from the System interface
export class ISystem
{
public:
    virtual ~ISystem() = default;
};

//the only singleton-- all major engine systems should be registered here.
//We also have a SystemManager for handling all of the systems, and automatically queued actions
//we will also have a central ECS for getting entity data and such
export class ServiceLocator
{
public:
    ServiceLocator()
    {
        //TODO: Figure out the amount of systems we're going to have at max
        systems.reserve(50);
    }

    template <typename SystemType>
    bool RegisterSystem(std::shared_ptr<SystemType>& system)
    {
        static_assert(std::derived_from<SystemType, ISystem> == true);
        auto [iter, inserted] = systems.insert({ 
            std::type_index(typeid(SystemType)), 
            std::weak_ptr(system) 
        });

        if (!inserted)
        {
            std::string err = "System has already been registered!: " + std::string(typeid(SystemType).name());
            std::cerr << err << std::endl;
            assert(inserted);
            return false;
        }
        return true;
    }
    
    template<class SystemType>
    std::weak_ptr<SystemType> Get()
    {
        static_assert(std::derived_from<SystemType, ISystem> == true);
        return GetSystem<SystemType>();
    }

    template<class SystemType>
    void Unregister()
    {
        GetSystem<SystemType>(); // Validates the system exists and is alive
        systems.erase(typeid(SystemType));
    }

    void Update()
    {
        // at the end of each frame check if we need to remove any systems.
        
        // While in debug mode, we want to make sure that developers
        // are properly deregistering and managing the object lifetimes correctly
        // in each system
#ifndef _DEBUG
        std::erase_if(systems,
            [](const auto& pair)
            {
                return pair.second.expired();
            });
#endif

    }

    static ServiceLocator* Instance()
    {
        static ServiceLocator instance;
        return &instance;
    }

    
private:
    std::unordered_map<std::type_index, std::weak_ptr<ISystem>> systems;

    template<class SystemType>
    std::weak_ptr<SystemType> GetSystem()
    {
        std::weak_ptr<ISystem> requested_system;
        //ERROR: array out of bounds
        try
        {
            requested_system = systems.at(typeid(SystemType));
        }
        catch (const std::exception& e)
        {
            std::string err = "System is not registered: " + std::string(typeid(SystemType).name());
            std::cerr << err << std::endl;
            std::cerr << e.what() << std::endl;
            assert(false);
        }
        //ERROR: this system is expired, and not properly deregistered
        if (bool expiry = requested_system.expired())
        {
            std::string err = "Get called on system that expired, but wasn't deregistered: " + std::string(typeid(SystemType).name());
            std::cerr << err << std::endl;
            assert(!expiry);
        }
        
        return std::weak_ptr<SystemType>(std::reinterpret_pointer_cast<SystemType>(requested_system.lock()));
    }
};