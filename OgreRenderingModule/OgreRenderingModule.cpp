// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"
#include <Ogre.h>
#include "OgreRenderingModule.h"
#include "ComponentRegistrarInterface.h"
#include "ServiceManager.h"
#include "ResourceHandler.h"
#include "OgreMeshResource.h"
#include "OgreTextureResource.h"
#include "EC_OgrePlaceable.h"
#include "EC_OgreMesh.h"
#include "EC_OgreLight.h"
#include "EC_OgreSky.h"
#include "EC_OgreCustomObject.h"
#include "EC_OgreConsoleOverlay.h"
#include "EC_OgreMovableTextOverlay.h"
#include "EC_OgreParticleSystem.h"
#include "EC_OgreAnimationController.h"
#include "EC_OgreEnvironment.h"
#include "InputEvents.h"
#include "SceneEvents.h"
#include "Entity.h"


namespace OgreRenderer
{
    OgreRenderingModule::OgreRenderingModule() : ModuleInterfaceImpl(type_static_),
        asset_event_category_(0),
        resource_event_category_(0),
        input_event_category_(0),
        scene_event_category_(0)
    {
    }

    OgreRenderingModule::~OgreRenderingModule()
    {
    }

    // virtual
    void OgreRenderingModule::Load()
    {
        using namespace OgreRenderer;

        LogInfo("Module " + Name() + " loaded.");

        DECLARE_MODULE_EC(EC_OgrePlaceable);
        DECLARE_MODULE_EC(EC_OgreMesh);
        DECLARE_MODULE_EC(EC_OgreLight);
        DECLARE_MODULE_EC(EC_OgreSky);
        DECLARE_MODULE_EC(EC_OgreCustomObject);
        DECLARE_MODULE_EC(EC_OgreConsoleOverlay);
        DECLARE_MODULE_EC(EC_OgreMovableTextOverlay);
        DECLARE_MODULE_EC(EC_OgreParticleSystem);
        DECLARE_MODULE_EC(EC_OgreAnimationController);
        DECLARE_MODULE_EC(EC_OgreEnvironment);

        AutoRegisterConsoleCommand(Console::CreateCommand(
                "RenderStats", "Prints out render statistics.", 
                Console::Bind(this, &OgreRenderingModule::ConsoleStats)));
    }

    // virtual
    void OgreRenderingModule::Unload()
    {
        LogInfo("Module " + Name() + " unloaded.");
    }

    // virtual
    void OgreRenderingModule::PreInitialize()
    {
        std::string ogre_config_filename = "ogre.cfg";

#if defined (_WINDOWS) && (_DEBUG)
        std::string plugins_filename = "pluginsd.cfg";
#elif defined (_WINDOWS)
        std::string plugins_filename = "plugins.cfg";
#else
        std::string plugins_filename = "plugins-unix.cfg";
#endif 
    
        // create renderer here, so it can be accessed in uninitialized state by other module's PreInitialize()

        std::string window_title = framework_->GetDefaultConfig().GetSetting<std::string>(Foundation::Framework::ConfigurationGroup(), "window_title") 
            + " " + VersionMajor() + "." + VersionMinor();

        renderer_ = OgreRenderer::RendererPtr(new OgreRenderer::Renderer(framework_, ogre_config_filename, plugins_filename, window_title));
    }

    // virtual
    void OgreRenderingModule::Initialize()
    { 
        assert (renderer_);

        assert (!renderer_->IsInitialized());
        renderer_->Initialize();
        
        framework_->GetServiceManager()->RegisterService(Foundation::Service::ST_Renderer, renderer_);

        LogInfo("Module " + Name() + " initialized.");
    }

    // virtual
    void OgreRenderingModule::PostInitialize()
    {
        Foundation::EventManagerPtr event_manager = framework_->GetEventManager();

        asset_event_category_ = event_manager->QueryEventCategory("Asset");
        
        if (asset_event_category_ == 0 )
            LogWarning("Unable to find event category for Asset events!");

        resource_event_category_ = event_manager->QueryEventCategory("Resource");
        input_event_category_ = event_manager->QueryEventCategory("Input");
        scene_event_category_ = event_manager->QueryEventCategory("Scene");

        renderer_->PostInitialize();
    }

    // virtual
    bool OgreRenderingModule::HandleEvent(
        Core::event_category_id_t category_id,
        Core::event_id_t event_id, 
        Foundation::EventDataInterface* data)
    {
        PROFILE(OgreRenderingModule_HandleEvent);
        if (!renderer_)
            return false;

        if (category_id == asset_event_category_)
        {
            return renderer_->GetResourceHandler()->HandleAssetEvent(event_id, data);
        }

        if (category_id == resource_event_category_)
        {
            return renderer_->GetResourceHandler()->HandleResourceEvent(event_id, data);
        }

        if (category_id == input_event_category_ && event_id == Input::Events::INWORLD_CLICK)
        {
            // do raycast into the world when user clicks mouse button
            Input::Events::Movement *movement = checked_static_cast<Input::Events::Movement*>(data);
            Foundation::RaycastResult result = renderer_->Raycast(movement->x_.abs_, movement->y_.abs_);
            
            Scene::Entity *entity = result.entity_;
                
            if (entity)
            {
                //std::cout << "Raycast hit entity " << entity << " pos " << result.pos_.x << " " << result.pos_.y << " " << result.pos_.z
                //          << " submesh " << result.submesh_ << " uv " << result.u_ << " " << result.v_ << std::endl;
                             
                Scene::Events::SceneEventData event_data(entity->GetId());
                framework_->GetEventManager()->SendEvent(scene_event_category_, Scene::Events::EVENT_ENTITY_GRAB, &event_data);
            }
        }

        return false;
    }

    // virtual 
    void OgreRenderingModule::Uninitialize()
    {        
        framework_->GetServiceManager()->UnregisterService(renderer_);
        renderer_.reset();
        
        LogInfo("Module " + Name() + " uninitialized.");
    }
    
    // virtual
    void OgreRenderingModule::Update(Core::f64 frametime)
    {
        {
            PROFILE(OgreRenderingModule_Update);
        
            renderer_->Update(frametime);
        }
        RESETPROFILER;
    }

    Console::CommandResult OgreRenderingModule::ConsoleStats(const Core::StringVector &params)
    {
        if (renderer_)
        {
            boost::shared_ptr<Console::ConsoleServiceInterface> console = GetFramework()->GetService<Console::ConsoleServiceInterface>(Foundation::Service::ST_Console).lock();
            if (console)
            {
                const Ogre::RenderTarget::FrameStats& stats = renderer_->GetCurrentRenderWindow()->getStatistics();
                console->Print("Average FPS: " + Core::ToString(stats.avgFPS));
                console->Print("Worst FPS: " + Core::ToString(stats.worstFPS));
                console->Print("Best FPS: " + Core::ToString(stats.bestFPS));

                console->Print("Triangles: " + Core::ToString(stats.triangleCount));
                console->Print("Batches: " + Core::ToString(stats.batchCount));

                return Console::ResultSuccess();
            }
        }

        return Console::ResultFailure("No renderer found.");
    }
}

extern "C" void POCO_LIBRARY_API SetProfiler(Foundation::Profiler *profiler);
void SetProfiler(Foundation::Profiler *profiler)
{
    Foundation::ProfilerSection::SetProfiler(profiler);
}


using namespace OgreRenderer;

POCO_BEGIN_MANIFEST(Foundation::ModuleInterface)
   POCO_EXPORT_CLASS(OgreRenderingModule)
   //POCO_EXPORT_CLASS(OgreGtkWindowModule)
POCO_END_MANIFEST

