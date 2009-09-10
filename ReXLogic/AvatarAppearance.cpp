// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"
#include "AvatarAppearance.h"
#include "LegacyAvatarSerializer.h"
#include "RexLogicModule.h"
#include "EC_AvatarAppearance.h"
#include "SceneManager.h"
#include "EC_OgreMesh.h"
#include "EC_OgreMovableTextOverlay.h"
#include "OgreMaterialResource.h"
#include "OgreMaterialUtils.h"
#include "Renderer.h"
#include "OgreConversionUtils.h"
#include <QDomDocument>
#include <QFile>

static const Core::Real FIXED_HEIGHT_OFFSET = -0.87f;
static const Core::Real OVERLAY_HEIGHT_MULTIPLIER = 1.5f;

namespace RexLogic
{
    AvatarAppearance::AvatarAppearance(RexLogicModule *rexlogicmodule) :
        rexlogicmodule_(rexlogicmodule)
    {
        std::string default_avatar_path = rexlogicmodule_->GetFramework()->GetDefaultConfig().DeclareSetting("RexAvatar", "default_avatar_file", std::string("./data/default_avatar.xml"));
        
        ReadDefaultAppearance(default_avatar_path);
    }

    AvatarAppearance::~AvatarAppearance()
    {
    }

    void AvatarAppearance::ReadDefaultAppearance(const std::string& filename)
    {
        default_appearance_ = boost::shared_ptr<QDomDocument>(new QDomDocument("defaultappearance"));
        
        QFile file(filename.c_str());
        if (!file.open(QIODevice::ReadOnly))
        {
            RexLogicModule::LogError("Could not open avatar default appearance file " + filename);
            return;
        }
        if (!default_appearance_->setContent(&file))
        {
            file.close();
            RexLogicModule::LogError("Could not load avatar default appearance file " + filename);
            return;
        }
        file.close();
    }
    
    void AvatarAppearance::SetupAppearance(Scene::EntityPtr entity)
    {
        if (!entity)
            return;
        
        Foundation::ComponentPtr meshptr = entity->GetComponent(OgreRenderer::EC_OgreMesh::NameStatic());
        Foundation::ComponentPtr appearanceptr = entity->GetComponent(EC_AvatarAppearance::NameStatic());
        if (!meshptr || !appearanceptr)
            return;
        
        OgreRenderer::EC_OgreMesh &mesh = *checked_static_cast<OgreRenderer::EC_OgreMesh*>(meshptr.get());
        EC_AvatarAppearance& appearance = *checked_static_cast<EC_AvatarAppearance*>(appearanceptr.get());
        
        // Deserialize appearance from the document into the EC
        LegacyAvatarSerializer::ReadAvatarAppearance(appearance, *default_appearance_);
        
        // Setup appearance
        SetupMeshAndMaterials(entity);
        SetupDynamicAppearance(entity);
        SetupAttachments(entity);
    }
    
    void AvatarAppearance::SetupDynamicAppearance(Scene::EntityPtr entity)
    {
        if (!entity)
            return;
        
        Foundation::ComponentPtr meshptr = entity->GetComponent(OgreRenderer::EC_OgreMesh::NameStatic());
        Foundation::ComponentPtr appearanceptr = entity->GetComponent(EC_AvatarAppearance::NameStatic());
        
        if (!meshptr || !appearanceptr)
            return;

        SetupMorphs(entity);
        SetupBoneModifiers(entity);
        AdjustHeightOffset(entity);
    }
    
    void AvatarAppearance::AdjustHeightOffset(Scene::EntityPtr entity)
    {
        if (!entity)
            return;
        
        Foundation::ComponentPtr meshptr = entity->GetComponent(OgreRenderer::EC_OgreMesh::NameStatic());
        Foundation::ComponentPtr appearanceptr = entity->GetComponent(EC_AvatarAppearance::NameStatic());
        
        if (!meshptr || !appearanceptr)
            return;
        
        OgreRenderer::EC_OgreMesh &mesh = *checked_static_cast<OgreRenderer::EC_OgreMesh*>(meshptr.get());
        EC_AvatarAppearance& appearance = *checked_static_cast<EC_AvatarAppearance*>(appearanceptr.get());
        
        Ogre::Vector3 offset = Ogre::Vector3::ZERO;
        Ogre::Vector3 initial_base_pos = Ogre::Vector3::ZERO;

        if (appearance.HasProperty("baseoffset"))
        {
            initial_base_pos = Ogre::StringConverter::parseVector3(appearance.GetProperty("baseoffset"));
        }

        if (appearance.HasProperty("basebone"))
        {
            Ogre::Bone* base_bone = GetAvatarBone(entity, appearance.GetProperty("basebone"));
            if (base_bone)
            {
                Ogre::Vector3 temp;
                GetInitialDerivedBonePosition(base_bone, temp);
                initial_base_pos += temp;
                offset = initial_base_pos;

                // Additionally, if has the rootbone property, can do dynamic adjustment for sitting etc.
                // and adjust the name overlay height
                if (appearance.HasProperty("rootbone"))
                {
                    Ogre::Bone* root_bone = GetAvatarBone(entity, appearance.GetProperty("rootbone"));
                    if (root_bone)
                    {
                        Ogre::Vector3 initial_root_pos;
                        Ogre::Vector3 current_root_pos = root_bone->_getDerivedPosition();
                        GetInitialDerivedBonePosition(root_bone, initial_root_pos);
                        
                        float c = abs(current_root_pos.y / initial_root_pos.y);
                        if (c > 1.0) c = 1.0;
                        offset = initial_base_pos * c;

                        // Set name overlay height according to base + root distance.
                        Foundation::ComponentPtr overlay = entity->GetComponent(OgreRenderer::EC_OgreMovableTextOverlay::NameStatic());
                        if (overlay)
                        {
                            OgreRenderer::EC_OgreMovableTextOverlay &name_overlay = *checked_static_cast<OgreRenderer::EC_OgreMovableTextOverlay*>(overlay.get());
                            name_overlay.SetOffset(Core::Vector3df(0, 0, abs(initial_base_pos.y - initial_root_pos.y) * OVERLAY_HEIGHT_MULTIPLIER));
                        }
                    }
                }
            }
        }

        mesh.SetAdjustPosition(Core::Vector3df(0.0f, 0.0f, -offset.y + FIXED_HEIGHT_OFFSET));
    }
    
    void AvatarAppearance::SetupMeshAndMaterials(Scene::EntityPtr entity)
    {
        OgreRenderer::EC_OgreMesh &mesh = *checked_static_cast<OgreRenderer::EC_OgreMesh*>(entity->GetComponent(OgreRenderer::EC_OgreMesh::NameStatic()).get());
        EC_AvatarAppearance& appearance = *checked_static_cast<EC_AvatarAppearance*>(entity->GetComponent(EC_AvatarAppearance::NameStatic()).get());
        
        // Mesh needs to be cloned if there are attachments which need to hide vertices
        bool need_mesh_clone = false;
        const AvatarAttachmentVector& attachments = appearance.GetAttachments();
        std::set<Core::uint> vertices_to_hide;
        for (Core::uint i = 0; i < attachments.size(); ++i)
        {
            if (attachments[i].vertices_to_hide_.size())
            {
                need_mesh_clone = true;
                for (Core::uint j = 0; j < attachments[i].vertices_to_hide_.size(); ++j)
                    vertices_to_hide.insert(attachments[i].vertices_to_hide_[j]);
            }
        }
        
        // Setup mesh
        //! \todo use mesh resource
        //! \todo handle setting custom skeleton
        mesh.SetMesh(appearance.GetMesh().name_, entity.get(), need_mesh_clone);
        if (need_mesh_clone)
            HideVertices(mesh.GetEntity(), vertices_to_hide);
        
        // Arbitrary materials would cause problems, because we really would want avatar materials to use the SuperShader style materials for
        // proper shadowing. For now, we force all materials to be based on LitTextured shader material
        AvatarMaterialVector materials = appearance.GetMaterials();
        for (Core::uint i = 0; i < materials.size(); ++i)
        {
            //! \todo handle multitextured materials, for now only one used (avatar generator only uses one texture per material)
            //! \todo use material/texture resources
            
            // See if a texture is specified, if not, assume default
            if (materials[i].textures_.size())
            {
                AvatarAsset& texture = materials[i].textures_[0];
                if (!texture.name_.empty())
                {
                    // Create a new temporary material resource for texture override. Should be deleted when the appearance EC is deleted
                    boost::shared_ptr<OgreRenderer::Renderer> renderer = rexlogicmodule_->GetFramework()->GetServiceManager()->
                        GetService<OgreRenderer::Renderer>(Foundation::Service::ST_Renderer).lock();
                    
                    Ogre::MaterialPtr override_mat = OgreRenderer::GetOrCreateLitTexturedMaterial(renderer->GetUniqueObjectName().c_str());
                    materials[i].asset_.resource_ = OgreRenderer::CreateResourceFromMaterial(override_mat);
                    
                    // Load local texture if not yet loaded
                    //! \todo remove once local avatar resources are not needed anymore
                    if (texture.id_.empty())
                    {
                        Ogre::TextureManager& tex_mgr = Ogre::TextureManager::getSingleton();
                        Ogre::TexturePtr tex = tex_mgr.getByName(texture.name_);
                        if (tex.isNull())
                        {
                            try
                            {
                                tex_mgr.load(texture.name_, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
                            }
                            catch (Ogre::Exception& e)
                            {
                                RexLogicModule::LogWarning("Local texture load failed: " + std::string(e.what()));
                            }
                        }
                        OgreRenderer::SetTextureUnitOnMaterial(override_mat, texture.name_);
                    }
                    
                    mesh.SetMaterial(i, override_mat->getName());
                }
            }
        }
        
        // Store the modified materials vector (with created temp resources) to the EC
        appearance.SetMaterials(materials);
        
        // Set adjustment orientation for mesh (Ogre meshes usually have Y-axis as vertical)
        Core::Quaternion adjust(Core::PI/2, 0, -Core::PI/2);
        mesh.SetAdjustOrientation(adjust);
        // Position approximately within the bounding box
        // Will be overridden by bone-based height adjust, if available
        mesh.SetAdjustPosition(Core::Vector3df(0.0f, 0.0f, FIXED_HEIGHT_OFFSET));
        mesh.SetCastShadows(true);
    }
    
    void AvatarAppearance::SetupAttachments(Scene::EntityPtr entity)
    {
        OgreRenderer::EC_OgreMesh &mesh = *checked_static_cast<OgreRenderer::EC_OgreMesh*>(entity->GetComponent(OgreRenderer::EC_OgreMesh::NameStatic()).get());
        EC_AvatarAppearance& appearance = *checked_static_cast<EC_AvatarAppearance*>(entity->GetComponent(EC_AvatarAppearance::NameStatic()).get());
        
        mesh.RemoveAllAttachments();
        
        const AvatarAttachmentVector& attachments = appearance.GetAttachments();
        
        for (Core::uint i = 0; i < attachments.size(); ++i)
        {
            // Setup attachment meshes
            //! \todo use mesh resources
            //! \todo use material & texture resources, force attachments to use shader materials?
            mesh.SetAttachmentMesh(i, attachments[i].mesh_.name_, attachments[i].bone_name_, attachments[i].link_skeleton_);
            mesh.SetAttachmentPosition(i, attachments[i].transform_.position_);
            mesh.SetAttachmentOrientation(i, attachments[i].transform_.orientation_);
            mesh.SetAttachmentScale(i, attachments[i].transform_.scale_);
        }
    }
    
    void AvatarAppearance::SetupMorphs(Scene::EntityPtr entity)
    {
        OgreRenderer::EC_OgreMesh &mesh = *checked_static_cast<OgreRenderer::EC_OgreMesh*>(entity->GetComponent(OgreRenderer::EC_OgreMesh::NameStatic()).get());
        EC_AvatarAppearance& appearance = *checked_static_cast<EC_AvatarAppearance*>(entity->GetComponent(EC_AvatarAppearance::NameStatic()).get());
        
        Ogre::Entity* ogre_entity = mesh.GetEntity();
        if (!ogre_entity)
            return;
        Ogre::AnimationStateSet* anims = ogre_entity->getAllAnimationStates();
        if (!anims)
            return;
            
        const MorphModifierVector& morphs = appearance.GetMorphModifiers();
        for (Core::uint i = 0; i < morphs.size(); ++i)
        {
            if (anims->hasAnimationState(morphs[i].morph_name_))
            {
                float timePos = morphs[i].value_;
                if (timePos < 0.0f)
                    timePos = 0.0f;
                // Clamp very close to 1.0, but do not actually go to 1.0 or the morph animation will wrap
                if (timePos > 0.99995f)
                    timePos = 0.99995f;
                
                Ogre::AnimationState* anim = anims->getAnimationState(morphs[i].morph_name_);
                anim->setTimePosition(timePos);
                anim->setEnabled(timePos > 0.0f);
                
                // Also set position in attachment entities, if have the same morph
                for (Core::uint j = 0; j < mesh.GetNumAttachments(); ++j)
                {
                    Ogre::Entity* attachment = mesh.GetAttachmentEntity(j);
                    if (!attachment)
                        continue;
                    Ogre::AnimationStateSet* attachment_anims = attachment->getAllAnimationStates();
                    if (!attachment_anims)
                        continue;
                    if (!attachment_anims->hasAnimationState(morphs[i].morph_name_))
                        continue;
                    Ogre::AnimationState* attachment_anim = attachment_anims->getAnimationState(morphs[i].morph_name_);
                    attachment_anim->setTimePosition(timePos);
                    attachment_anim->setEnabled(timePos > 0.0f);
                }
            }
        }
    }
    
    void AvatarAppearance::SetupBoneModifiers(Scene::EntityPtr entity)
    {
        OgreRenderer::EC_OgreMesh &mesh = *checked_static_cast<OgreRenderer::EC_OgreMesh*>(entity->GetComponent(OgreRenderer::EC_OgreMesh::NameStatic()).get());
        EC_AvatarAppearance& appearance = *checked_static_cast<EC_AvatarAppearance*>(entity->GetComponent(EC_AvatarAppearance::NameStatic()).get());
        
        ResetBones(entity);
        
        const BoneModifierSetVector& bone_modifiers = appearance.GetBoneModifiers();
        for (Core::uint i = 0; i < bone_modifiers.size(); ++i)
        {
            for (Core::uint j = 0; j < bone_modifiers[i].modifiers_.size(); ++j)
            {
                ApplyBoneModifier(entity, bone_modifiers[i].modifiers_[j], bone_modifiers[i].value_);
            }
        }
    }
    
    void AvatarAppearance::ResetBones(Scene::EntityPtr entity)
    {
        OgreRenderer::EC_OgreMesh &mesh = *checked_static_cast<OgreRenderer::EC_OgreMesh*>(entity->GetComponent(OgreRenderer::EC_OgreMesh::NameStatic()).get());
        EC_AvatarAppearance& appearance = *checked_static_cast<EC_AvatarAppearance*>(entity->GetComponent(EC_AvatarAppearance::NameStatic()).get());
        
        Ogre::Entity* ogre_entity = mesh.GetEntity();
        if (!ogre_entity)
            return;
        // See that we actually have a skeleton
        Ogre::SkeletonInstance* skeleton = ogre_entity->getSkeleton();
        Ogre::Skeleton* orig_skeleton = ogre_entity->getMesh()->getSkeleton().get();
        if ((!skeleton) || (!orig_skeleton))
            return;
        
        if (skeleton->getNumBones() != orig_skeleton->getNumBones())
            return;
        
        for (Core::uint i = 0; i < orig_skeleton->getNumBones(); ++i)
        {
            Ogre::Bone* bone = skeleton->getBone(i);
            Ogre::Bone* orig_bone = orig_skeleton->getBone(i);

            bone->setPosition(orig_bone->getInitialPosition());
            bone->setOrientation(orig_bone->getInitialOrientation());
            bone->setScale(orig_bone->getInitialScale());
            bone->setInitialState();
        }
    }
    
    void AvatarAppearance::ApplyBoneModifier(Scene::EntityPtr entity, const BoneModifier& modifier, Core::Real value)
    {
        OgreRenderer::EC_OgreMesh &mesh = *checked_static_cast<OgreRenderer::EC_OgreMesh*>(entity->GetComponent(OgreRenderer::EC_OgreMesh::NameStatic()).get());
        EC_AvatarAppearance& appearance = *checked_static_cast<EC_AvatarAppearance*>(entity->GetComponent(EC_AvatarAppearance::NameStatic()).get());
        
        Ogre::Entity* ogre_entity = mesh.GetEntity();
        if (!ogre_entity)
            return;
        // See that we actually have a skeleton
        Ogre::SkeletonInstance* skeleton = ogre_entity->getSkeleton();
        Ogre::Skeleton* orig_skeleton = ogre_entity->getMesh()->getSkeleton().get();
        if ((!skeleton) || (!orig_skeleton))
            return;
        
        if ((!skeleton->hasBone(modifier.bone_name_)) || (!orig_skeleton->hasBone(modifier.bone_name_)))
            return; // Bone not found, nothing to do
            
        Ogre::Bone* bone = skeleton->getBone(modifier.bone_name_);
        Ogre::Bone* orig_bone = orig_skeleton->getBone(modifier.bone_name_);
        
        if (value < 0.0f)
            value = 0.0f;
        if (value > 1.0f)
            value = 1.0f;
        
        // Rotation
        {
            Ogre::Matrix3 rot_start, rot_end, rot_base, rot_orig;
            Ogre::Radian sx, sy, sz;
            Ogre::Radian ex, ey, ez;
            Ogre::Radian bx, by, bz;
            Ogre::Radian rx, ry, rz;
            OgreRenderer::ToOgreQuaternion(modifier.start_.orientation_).ToRotationMatrix(rot_start);
            OgreRenderer::ToOgreQuaternion(modifier.end_.orientation_).ToRotationMatrix(rot_end);
            bone->getInitialOrientation().ToRotationMatrix(rot_orig);
            rot_start.ToEulerAnglesXYZ(sx, sy, sz);
            rot_end.ToEulerAnglesXYZ(ex, ey, ez);
            rot_orig.ToEulerAnglesXYZ(rx, ry, rz);
            
            switch (modifier.orientation_mode_)
            {
            case Absolute:
                bx = 0;
                by = 0;
                bz = 0;
                break;
                
            case Relative:
                orig_bone->getInitialOrientation().ToRotationMatrix(rot_base);
                rot_base.ToEulerAnglesXYZ(bx, by, bz);
                break;
                
            case Cumulative:
                bone->getInitialOrientation().ToRotationMatrix(rot_base);
                rot_base.ToEulerAnglesXYZ(bx, by, bz);
                break;
            }
            
            if (sx != Ogre::Radian(0) || ex != Ogre::Radian(0))
                rx = bx + sx * (1.0 - value) + ex * (value);
            if (sy != Ogre::Radian(0) || ey != Ogre::Radian(0))
                ry = by + sy * (1.0 - value) + ey * (value);
            if (sz != Ogre::Radian(0) || ez != Ogre::Radian(0))
                rz = bz + sz * (1.0 - value) + ez * (value);
            
            Ogre::Matrix3 rot_new;
            rot_new.FromEulerAnglesXYZ(rx, ry, rz);
            Ogre::Quaternion q_new(rot_new);
            bone->setOrientation(Ogre::Quaternion(rot_new));
        }
        
        // Translation
        {
            Core::Real sx = modifier.start_.position_.x;
            Core::Real sy = modifier.start_.position_.y;
            Core::Real sz = modifier.start_.position_.z;
            Core::Real ex = modifier.end_.position_.x;
            Core::Real ey = modifier.end_.position_.y;
            Core::Real ez = modifier.end_.position_.z;
            
            Ogre::Vector3 trans, base;
            trans = bone->getInitialPosition();
            switch (modifier.position_mode_)
            {
            case Absolute:
                base = Ogre::Vector3(0,0,0);
                break;
            case Relative:
                base = orig_bone->getInitialPosition();
                break;
            }
            
            if (sx != 0 || ex != 0)
                trans.x = base.x + sx * (1.0 - value) + ex * value;
            if (sy != 0 || ey != 0)
                trans.y = base.y + sy * (1.0 - value) + ey * value;
            if (sz != 0 || ez != 0)
                trans.z = base.z + sz * (1.0 - value) + ez * value;
            
            bone->setPosition(trans);
        }
        
        // Scale
        {
            Ogre::Vector3 scale = bone->getInitialScale();
            Core::Real sx = modifier.start_.scale_.x;
            Core::Real sy = modifier.start_.scale_.y;
            Core::Real sz = modifier.start_.scale_.z;
            Core::Real ex = modifier.end_.scale_.x;
            Core::Real ey = modifier.end_.scale_.y;
            Core::Real ez = modifier.end_.scale_.z;
            
            if (sx != 1 || ex != 1)
                scale.x = sx * (1.0 - value) + ex * value;
            if (sy != 1 || ey != 1)
                scale.y = sy * (1.0 - value) + ey * value;
            if (sz != 1 || ez != 1)
                scale.z = sz * (1.0 - value) + ez * value;
            
            bone->setScale(scale);
        }
        
        bone->setInitialState();
    }

    void AvatarAppearance::GetInitialDerivedBonePosition(Ogre::Node* bone, Ogre::Vector3& position)
    {
        // Hacky and slow way to derive the initial position of the base bone. Do not use current position
        // because animations change it
        position = bone->getInitialPosition();
        Ogre::Vector3 scale = bone->getInitialScale();
        Ogre::Quaternion orient = bone->getInitialOrientation();

        while (bone->getParent())
        {
           Ogre::Node* parent = bone->getParent();

           if (bone->getInheritOrientation())
           {
              orient = parent->getInitialOrientation() * orient;
           }
           if (bone->getInheritScale())
           {
              scale = parent->getInitialScale() * scale;
           }

           position = parent->getInitialOrientation() * (parent->getInitialScale() * position);
           position += parent->getInitialPosition();

           bone = parent;
        }
    }
    
    Ogre::Bone* AvatarAppearance::GetAvatarBone(Scene::EntityPtr entity, const std::string& bone_name)
    {
        Foundation::ComponentPtr meshptr = entity->GetComponent(OgreRenderer::EC_OgreMesh::NameStatic());
        if (!meshptr)
            return 0;
        
        OgreRenderer::EC_OgreMesh &mesh = *checked_static_cast<OgreRenderer::EC_OgreMesh*>(meshptr.get());
        Ogre::Entity* ogre_entity = mesh.GetEntity();
        if (!ogre_entity)
            return 0;
        Ogre::SkeletonInstance* skeleton = ogre_entity->getSkeleton();
        if (!skeleton)
            return 0;
        if (!skeleton->hasBone(bone_name))
            return 0;
        return skeleton->getBone(bone_name);
    }
    
    void AvatarAppearance::HideVertices(Ogre::Entity* entity, std::set<Core::uint> vertices_to_hide)
    {
        if (!entity)
            return;
        Ogre::MeshPtr mesh = entity->getMesh();
        if (mesh.isNull())
            return;
        
        for (Core::uint m = 0; m < 1; ++m)
        {
            // Under current system, it seems vertices should only be hidden from first submesh
            Ogre::SubMesh *submesh = mesh->getSubMesh(m);
            Ogre::IndexData *data = submesh->indexData;
            Ogre::HardwareIndexBufferSharedPtr ibuf = data->indexBuffer;

            unsigned long* lIdx = static_cast<unsigned long*>(ibuf->lock(Ogre::HardwareBuffer::HBL_NORMAL));
            unsigned short* pIdx = reinterpret_cast<unsigned short*>(lIdx);
            bool use32bitindexes = (ibuf->getType() == Ogre::HardwareIndexBuffer::IT_32BIT);

            for (Core::uint n = 0; n < data->indexCount; n += 3)
            {
                if (!use32bitindexes)
                {
                    if (vertices_to_hide.find(pIdx[n]) != vertices_to_hide.end() ||
                        vertices_to_hide.find(pIdx[n+1]) != vertices_to_hide.end() ||
                        vertices_to_hide.find(pIdx[n+2]) != vertices_to_hide.end())
                    {
                        if (n + 3 < data->indexCount)
                        {
                            for (size_t i = n ; i<data->indexCount-3 ; ++i)
                            {
                                pIdx[i] = pIdx[i+3];
                            }
                        }
                        data->indexCount -= 3;
                        n -= 3;
                    }
                }
                else
                {
                    if (vertices_to_hide.find(lIdx[n]) != vertices_to_hide.end() ||
                        vertices_to_hide.find(lIdx[n+1]) != vertices_to_hide.end() ||
                        vertices_to_hide.find(lIdx[n+2]) != vertices_to_hide.end())
                    {
                        if (n + 3 < data->indexCount)
                        {
                            for (size_t i = n ; i<data->indexCount-3 ; ++i)
                            {
                                lIdx[i] = lIdx[i+3];
                            }
                        }
                        data->indexCount -= 3;
                        n -= 3;
                    }
                }
            }
            ibuf->unlock();
        }
    }
}