#include "FBXImporter.h"

#include "Private/FBXAnimationImport.h"
#include "Private/FBXMaterialImport.h"
#include "Private/FBXMeshImport.h"
#include "Private/FBXSceneImport.h"
#include "Private/FBXSkeletonImport.h"
#include "Private/FBXUtils.h"

#include "Scene3D/Scene.h"
#include "Scene3D/SceneUtils.h"

namespace DAVA
{

Scene* FBXImporter::ConstructSceneFromFBX(const FilePath& fbxPath)
{
    FbxManager* fbxManager = FbxManager::Create();

    Scene* scene = nullptr;
    FbxScene* fbxScene = FBXImporterDetails::ImportFbxScene(fbxManager, fbxPath);
    if (fbxScene != nullptr)
    {
        FbxGeometryConverter fbxGeometryConverter(fbxManager);
        fbxGeometryConverter.Triangulate(fbxScene, true); //Triangulate whole scene

        scene = new Scene();
        FBXImporterDetails::ProcessSceneSkeletonsRecursive(fbxScene->GetRootNode());
        FBXImporterDetails::ProcessSceneHierarchyRecursive(fbxScene->GetRootNode(), scene);
        fbxScene->Destroy();
    }
    fbxManager->Destroy();

    FBXImporterDetails::ClearMaterialCache();
    FBXImporterDetails::ClearMeshCache();
    FBXImporterDetails::ClearSkeletonCache();

    return scene;
}

bool FBXImporter::ConvertToSC2(const FilePath& fbxPath, const FilePath& sc2Path)
{
    Scene* scene = ConstructSceneFromFBX(fbxPath);
    if (scene != nullptr)
    {
        bool combinedSuccessfull = SceneUtils::CombineLods(scene);
        if (combinedSuccessfull)
        {
            scene->SaveScene(sc2Path);
            return true;
        }
    }

    return false;
}

bool FBXImporter::ConvertAnimations(const FilePath& fbxPath)
{
    FbxManager* fbxManager = FbxManager::Create();

    FbxScene* fbxScene = FBXImporterDetails::ImportFbxScene(fbxManager, fbxPath);
    if (fbxScene != nullptr)
    {
        Vector<FBXImporterDetails::FBXAnimationStackData> animations = FBXImporterDetails::ImportAnimations(fbxScene);

        for (FBXImporterDetails::FBXAnimationStackData& animation : animations)
        {
            FilePath animationPath = (animations.size() == 1) ? FilePath::CreateWithNewExtension(fbxPath, ".anim") : FilePath::CreateWithNewExtension(fbxPath, animation.name + ".anim");
            FBXImporterDetails::SaveAnimation(animation, animationPath);
        }

        fbxScene->Destroy();
    }
    fbxManager->Destroy();

    return (fbxScene != nullptr);
}

}; //ns DAVA
