module;

export module ServiceLocator;
import std;
// all systems should inherit from the System interface
export class ISystem
{
public:
    virtual ~ISystem() = default;
    std::string name;    
};

//the only singleton-- all major engine systems should be registered here.
//We also have a SystemManager for handling all of the systems, and automatically queued actions
//we will also have a central ECS for getting entity data and such
export class ServiceLocator
{
public:
    ServiceLocator();
    bool RegisterSystem(const std::shared_ptr<ISystem>& system )
    {
        if (!system) return false;
        systems.push_back(system);
        return true;
    }
    
    template<class T>
    std::shared_ptr<T> GetSystem()
    {
        static_assert(std::is_base_of_v<ISystem, T>, "T must derive from ISystem");
        
        for (auto& weakSystem : systems)
        {
            if (auto system = weakSystem.lock())  // Convert weak_ptr to shared_ptr
            {
                if (auto derived = std::dynamic_pointer_cast<T>(system))
                {
                    return derived;
                }
            }
        }
        return nullptr;  // System not found
    }
private:
    //don't own the references, just keep track
    std::vector<std::weak_ptr<ISystem>> systems;
};