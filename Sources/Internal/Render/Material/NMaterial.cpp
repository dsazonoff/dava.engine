/*==================================================================================
    Copyright (c) 2008, binaryzebra
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    * Neither the name of the binaryzebra nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE binaryzebra AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL binaryzebra BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
=====================================================================================*/


#include "Render/Material/MaterialSystem.h"
#include "Render/Material/NMaterial.h"
#include "Render/RenderManager.h"
#include "Render/RenderState.h"
#include "Render/3D/PolygonGroup.h"
#include "Render/RenderBase.h"
#include "FileSystem/YamlParser.h"
#include "Render/Shader.h"
#include "Render/Material/MaterialGraph.h"
#include "Render/Material/MaterialCompiler.h"
#include "Render/ShaderCache.h"
#include "Render/Highlevel/Camera.h"
#include "Render/Highlevel/Light.h"
#include "Scene3D/SceneFile/SerializationContext.h"
#include "Utils/StringFormat.h"

namespace DAVA
{
    
    
NMaterialDescriptor::NMaterialDescriptor()
{
    
}
    
NMaterialDescriptor::~NMaterialDescriptor()
{
    
}

uint32 NMaterialDescriptor::GetTextureSlotByName(const String & textureName)
{
    Map<String, uint32>::iterator it = slotNameMap.find(textureName);
    if (it != slotNameMap.end())
    {
        return it->second;
    }
    // if we haven't found slot let's use first slot
    return 0;
}
    
uint32 NMaterialDescriptor::GetUniformSlotByName(const String & uniformName)
{
    Map<String, uint32>::iterator it = uniformNameMap.find(uniformName);
    if (it != uniformNameMap.end())
    {
        return it->second;
    }
    // if we haven't found slot let's use first slot
    return 0;
}
    
void NMaterialDescriptor::SetNameForTextureSlot(uint32 slot, const String & name)
{
    slotNameMap[name] = slot;
}

void NMaterialDescriptor::SetNameForUniformSlot(uint32 slot, const String & name)
{
    uniformNameMap[name] = slot;
}

MaterialTechnique::MaterialTechnique(const FastName & _shaderName, const FastNameSet & _uniqueDefines, RenderState * _renderState)
{
    shader = 0;
    shaderName = _shaderName;
    uniqueDefines = _uniqueDefines;
    renderState = _renderState;
}
    
MaterialTechnique::~MaterialTechnique()
{
    SafeRelease(shader);
}

void MaterialTechnique::RecompileShader(const FastNameSet& materialDefines)
{
	FastNameSet combinedDefines = materialDefines;
	
	if(uniqueDefines.Size() > 0)
	{
		combinedDefines.Combine(uniqueDefines);
	}
	
	SafeRelease(shader);
    shader = SafeRetain(ShaderCache::Instance()->Get(shaderName, combinedDefines));
}
    
const FastName NMaterial::TEXTURE_ALBEDO("albedo");
const FastName NMaterial::TEXTURE_NORMAL("normal");
const FastName NMaterial::TEXTURE_DETAIL("detail");
const FastName NMaterial::TEXTURE_LIGHTMAP("lightmap");
const FastName NMaterial::TEXTURE_DECAL("decal");
	
static FastName TEXTURE_NAME_PROPS[] = {
	NMaterial::TEXTURE_ALBEDO,
	NMaterial::TEXTURE_NORMAL,
	NMaterial::TEXTURE_DETAIL,
	NMaterial::TEXTURE_LIGHTMAP,
	NMaterial::TEXTURE_DECAL
};

NMaterial::NMaterial()
{
    activeTechnique = 0;
	ready = false;
	materialDynamicLit = false;
	configMaterial = false;
}
    
NMaterial::~NMaterial()
{
	//VI: TODO: It would be really great to remove itself from the material system here
	//VI: but it's not really good to store pointer to material system in each material
	
	SetParent(NULL);
	
	for(size_t i = 0; i < children.size(); ++i)
	{
		children[i]->SetParent(NULL);
	}
	
	for(HashMap<FastName, TextureBucket*>::Iterator i = state.textures.Begin();
		i != state.textures.End();
		++i)
	{
		SafeRelease(i.GetValue()->texture);
		delete i.GetValue();
	}
	
	state.textures.Clear();
	texturesArray.clear();
}
    
void NMaterial::AddMaterialProperty(const String & keyName, const YamlNode * uniformNode)
{
    FastName uniformName = keyName;
    Logger::Debug("Uniform Add:%s %s", keyName.c_str(), uniformNode->AsString().c_str());
    
    Shader::eUniformType type = Shader::UT_FLOAT;
    union 
    {
        float val;
        float valArray[4];
        float valMatrix[4 * 4];
        int32 valInt;
    }data;
    
    uint32 size = 0;
    
    if (uniformNode->GetType() == YamlNode::TYPE_STRING)
    {
        String uniformValue = uniformNode->AsString();
        if (uniformValue.find('.') != String::npos)
            type = Shader::UT_FLOAT;
        else
            type = Shader::UT_INT;
    }
    else if (uniformNode->GetType() == YamlNode::TYPE_ARRAY)
    {
        size = uniformNode->GetCount();
        uint32 arrayCount = 0;
        for (uint32 k = 0; k < size; ++k)
        {
            if (uniformNode->Get(k)->GetType() == YamlNode::TYPE_ARRAY)
            {
                arrayCount++;
            }
        }
        if (size == arrayCount)
        {
            if (size == 2)type = Shader::UT_FLOAT_MAT2;
            else if (size == 3)type = Shader::UT_FLOAT_MAT3;
            else if (size == 4)type = Shader::UT_FLOAT_MAT4;
        }else if (arrayCount == 0)
        {
            if (size == 2)type = Shader::UT_FLOAT_VEC2;
            else if (size == 3)type = Shader::UT_FLOAT_VEC3;
            else if (size == 4)type = Shader::UT_FLOAT_VEC4;
        }else
        {
            DVASSERT(0 && "Something went wrong");
        }
    }
    
 
    
    switch (type) {
        case Shader::UT_INT:
            data.valInt = uniformNode->AsInt();
            break;
        case Shader::UT_FLOAT:
            data.val = uniformNode->AsFloat();
            break;
        case Shader::UT_FLOAT_VEC2:
        case Shader::UT_FLOAT_VEC3:
        case Shader::UT_FLOAT_VEC4:
            for (uint32 k = 0; k < size; ++k)
            {
                data.valArray[k] = uniformNode->Get(k)->AsFloat();
            }
            break;
            
        default:
            Logger::Error("Wrong material property or format not supported.");
            break;
    }
    
    NMaterialProperty * materialProperty = new NMaterialProperty();
    materialProperty->size = 1;
    materialProperty->type = type;
    materialProperty->data = new char[Shader::GetUniformTypeSize(type)];
    
    memcpy(materialProperty->data, &data, Shader::GetUniformTypeSize(type));
    
    state.materialProperties.Insert(uniformName, materialProperty);
}
    
void NMaterial::SetPropertyValue(const FastName & propertyFastName, Shader::eUniformType type, uint32 size, const void * data)
{
    NMaterialProperty * materialProperty = state.materialProperties.GetValue(propertyFastName);
    if (materialProperty)
    {
        if (materialProperty->type != type || materialProperty->size != size)
        {
			char* tmpPtr = static_cast<char*>(materialProperty->data);
            SafeDeleteArray(tmpPtr); //cannot delete void* pointer
            materialProperty->size = size;
            materialProperty->type = type;
            materialProperty->data = new char[Shader::GetUniformTypeSize(type) * size];
        }
    }
	else
    {
        materialProperty = new NMaterialProperty;
        materialProperty->size = size;
        materialProperty->type = type;
        materialProperty->data = new char[Shader::GetUniformTypeSize(type) * size];
        state.materialProperties.Insert(propertyFastName, materialProperty);
    }

    memcpy(materialProperty->data, data, size * Shader::GetUniformTypeSize(type));
}
	
NMaterialProperty* NMaterial::GetMaterialProperty(const FastName & keyName)
{
	NMaterial * currentMaterial = this;
    NMaterialProperty * property = NULL;
    while(currentMaterial != 0)
    {
		property = currentMaterial->state.materialProperties.GetValue(keyName);
        if (property)
        {
			break;
		}
		
		currentMaterial = currentMaterial->state.parent;
    }
	
	return property;
}
	
void NMaterial::SetMaterialName(const String& name)
{
	state.materialName = name;
}

bool NMaterial::LoadFromFile(const String & pathname)
{
    YamlParser * parser = YamlParser::Create(pathname);
    if (!parser)
    {
        Logger::Error("Can't load requested material: %s", pathname.c_str());
        return false;
    }
    YamlNode * rootNode = parser->GetRootNode();
    
    if (!rootNode)
    {
        SafeRelease(rootNode);
		SafeRelease(parser);
        return false;
    }
    
    const YamlNode * materialNode = rootNode->Get("Material");

    const YamlNode * layersNode = materialNode->Get("Layers");
    if (layersNode)
    {
        int32 count = layersNode->GetCount();
        for (int32 k = 0; k < count; ++k)
        {
            const YamlNode * singleLayerNode = layersNode->Get(k);
            state.layers.Insert(FastName(singleLayerNode->AsString()));
        }
    }
	effectiveLayers.Combine(state.layers);
    
    const YamlNode * uniformsNode = materialNode->Get("Uniforms");
    if (uniformsNode)
    {
        uint32 count = uniformsNode->GetCount();
        for (uint32 k = 0; k < count; ++k)
        {
            const YamlNode * uniformNode = uniformsNode->Get(k);
            if (uniformNode)
            {
                AddMaterialProperty(uniformsNode->GetItemKeyName(k), uniformNode);
            }
        }
    }

	const YamlNode * materialDefinesNode = materialNode->Get("MaterialDefines");
    if (materialDefinesNode)
    {
        uint32 count = materialDefinesNode->GetCount();
        for (uint32 k = 0; k < count; ++k)
        {
            const YamlNode * defineNode = materialDefinesNode->Get(k);
            if (defineNode)
            {
                state.nativeDefines.Insert(FastName(defineNode->AsString().c_str()));
            }
        }
    }
	
    for (int32 k = 0; k < rootNode->GetCount(); ++k)
    {
        const YamlNode * renderStepNode = rootNode->Get(k);
        
        if (renderStepNode->AsString() == "RenderPass")
        {
            Logger::Debug("- RenderPass found: %s", renderStepNode->AsString().c_str());
            const YamlNode * shaderNode = renderStepNode->Get("Shader");
            const YamlNode * shaderGraphNode = renderStepNode->Get("ShaderGraph");

            if (!shaderNode && !shaderGraphNode)
            {
                Logger::Error("Material:%s RenderPass:%s does not have shader or shader graph", pathname.c_str(), renderStepNode->AsString().c_str());
                SafeRelease(parser);
                return false;
            }
            
            FastName shaderName;
            if (shaderNode)
            {
                shaderName = FastName(shaderNode->AsString().c_str());
            }
            
            if (shaderGraphNode)
            {
                String shaderGraphPathname = shaderGraphNode->AsString();
                MaterialGraph * graph = new MaterialGraph();
                graph->LoadFromFile(shaderGraphPathname);

                MaterialCompiler * compiler = new MaterialCompiler();
                compiler->Compile(graph, 0, 4, 0);
                
                //vertexShader = compiler->GetCompiledVertexShaderPathname();
                //fragmentShader = compiler->GetCompiledFragmentShaderPathname();
                
                SafeRelease(compiler);
                SafeRelease(graph);
            }
            
            FastNameSet definesSet;
            const YamlNode * definesNode = renderStepNode->Get("UniqueDefines");
            if (definesNode)
            {
                int32 count = definesNode->GetCount();
                for (int32 k = 0; k < count; ++k)
                {
                    const YamlNode * singleDefineNode = definesNode->Get(k);
                    definesSet.Insert(FastName(singleDefineNode->AsString().c_str()));
                }
            }
            
            RenderState * renderState = new RenderState();
            const YamlNode * renderStateNode = renderStepNode->Get("RenderState");
            if (renderStepNode)
            {
                renderState->LoadFromYamlNode(renderStepNode);
            }
            
            
            const YamlNode * renderPassNameNode = renderStepNode->Get("Name");
            FastName renderPassName;
            if (renderPassNameNode)
            {
                renderPassName = renderPassNameNode->AsString();
            }
                        
            MaterialTechnique * technique = new MaterialTechnique(shaderName, definesSet, renderState);
            AddMaterialTechnique(renderPassName, technique);
        }
    }

    SafeRelease(parser);
    return true;
}
    
void NMaterial::AddMaterialTechnique(const FastName & techniqueName, MaterialTechnique * materialTechnique)
{
    state.techniqueForRenderPass.Insert(techniqueName, materialTechnique);
}
    
MaterialTechnique * NMaterial::GetTechnique(const FastName & techniqueName)
{
    MaterialTechnique * technique = state.techniqueForRenderPass.GetValue(techniqueName);
/*  
    if (!technique)
    {
        NMaterial * currentMaterial = parent;
        while(currentMaterial != 0)
        {
            technique = currentMaterial->techniqueForRenderPass.GetValue(techniqueName);
            if (technique)
            {
                // TODO: Find effective way to store parent techniques in children, to avoid cycling through them
                // As a first decision we can use just store of this technique in child material map.
                break;
            }
            currentMaterial = currentMaterial->parent;
        }
    }
*/
    DVASSERT(technique != 0);
    return technique;
}
    
    
void NMaterial::SetTexture(const FastName & textureFastName, Texture * texture)
{
	TextureBucket* bucket = state.textures.GetValue(textureFastName);
	if(NULL != bucket)
	{
		DVASSERT(bucket->index < texturesArray.size());
		
		if(bucket->texture != texture)
		{
			SafeRelease(bucket->texture);
			bucket->texture = SafeRetain(texture);
			texturesArray[bucket->index] = texture;
		}
	}
	else
	{
		TextureBucket* bucket = new TextureBucket();
		bucket->texture = SafeRetain(texture);
		bucket->index = texturesArray.size();
		
		state.textures.Insert(textureFastName, bucket);
		texturesArray.push_back(texture);
		textureNamesArray.push_back(textureFastName);
		textureSlotArray.push_back(-1);
	}
}
    
Texture * NMaterial::GetTexture(const FastName & textureFastName) const
{
    TextureBucket* bucket = state.textures.GetValue(textureFastName);
    if (!bucket)
    {
        NMaterial * currentMaterial = state.parent;
        while(currentMaterial != 0)
        {
            bucket = currentMaterial->state.textures.GetValue(textureFastName);
            if (bucket)
            {
                // TODO: Find effective way to store parent techniques in children, to avoid cycling through them
                // As a first decision we can use just store of this technique in child material map.
                break;
            }
            currentMaterial = currentMaterial->state.parent;
        }
    }
    return (bucket != NULL) ? bucket->texture : NULL;
}
    
Texture * NMaterial::GetTexture(uint32 index)
{
    return texturesArray[index];
}

uint32 NMaterial::GetTextureCount()
{
    return (uint32)texturesArray.size();
}
    
void NMaterial::BindMaterialTechnique(const FastName & techniqueName, Camera* camera)
{
	if(!ready)
	{
		Rebuild(false);
	}
	
    if (techniqueName != activeTechniqueName)
    {
        activeTechnique = GetTechnique(techniqueName);
        if (activeTechnique)
            activeTechniqueName = techniqueName;
    }
	
	SetupPerFrameProperties(camera);
    
	RenderState* renderState = activeTechnique->GetRenderState();
	
	NMaterial* texMat = this;
	while(texMat)
	{
		BindTextures(texMat, renderState);
		texMat = texMat->state.parent;
	}
	
    Shader * shader = activeTechnique->GetShader();	
    renderState->SetShader(shader);

    RenderManager::Instance()->FlushState(renderState);

    uint32 uniformCount = shader->GetUniformCount();
    for (uint32 uniformIndex = 0; uniformIndex < uniformCount; ++uniformIndex)
    {
        Shader::Uniform * uniform = shader->GetUniform(uniformIndex);
        if (uniform->id == Shader::UNIFORM_NONE)
        {
            NMaterial * currentMaterial = this;
            
            while(currentMaterial != 0)
            {
                NMaterialProperty * property = currentMaterial->state.materialProperties.GetValue(uniform->name);
                if (property)
                {
                    shader->SetUniformValueByIndex(uniformIndex, uniform->type, uniform->size, property->data);
                    break;
                }
                currentMaterial = currentMaterial->state.parent;
            }
        }
    }
}
	
void NMaterial::BindTextures(NMaterial* curMaterial, RenderState* rs)
{
	size_t textureCount = curMaterial->texturesArray.size();
	for(size_t i = 0; i < textureCount; ++i)
	{
		int32 textureSlot = curMaterial->textureSlotArray[i];
		if(textureSlot < 0)
		{
			NMaterialProperty* prop = curMaterial->GetMaterialProperty(curMaterial->textureNamesArray[i]);			
			DVASSERT(prop);
			
			textureSlot = *(int32*)prop->data;
			curMaterial->textureSlotArray[i] = textureSlot;
		}
		
		rs->SetTexture(curMaterial->texturesArray[i], textureSlot);
	}
}

void NMaterial::Draw(PolygonGroup * polygonGroup)
{
    // TODO: Remove support of OpenGL ES 1.0 from attach render data
    RenderManager::Instance()->SetRenderData(polygonGroup->renderDataObject);
	RenderManager::Instance()->AttachRenderData();

    // TODO: rethink this code
    if (polygonGroup->renderDataObject->GetIndexBufferID() != 0)
    {
        RenderManager::Instance()->HWDrawElements(PRIMITIVETYPE_TRIANGLELIST, polygonGroup->indexCount, EIF_16, 0);
    }
    else
    {
        RenderManager::Instance()->HWDrawElements(PRIMITIVETYPE_TRIANGLELIST, polygonGroup->indexCount, EIF_16, polygonGroup->indexArray);
    }
}
	
void NMaterial::Draw(RenderDataObject*	renderData, uint16* indices, uint16 indexCount)
{
	DVASSERT(renderData);
	
	// TODO: Remove support of OpenGL ES 1.0 from attach render data
    RenderManager::Instance()->SetRenderData(renderData);
	RenderManager::Instance()->AttachRenderData();
	
    // TODO: rethink this code
    if (renderData->GetIndexBufferID() != 0)
    {
        RenderManager::Instance()->HWDrawElements(PRIMITIVETYPE_TRIANGLELIST, renderData->indexCount, EIF_16, 0);
    }
    else
    {
		if(renderData->indexCount)
		{
			RenderManager::Instance()->HWDrawElements(PRIMITIVETYPE_TRIANGLELIST, renderData->indexCount, EIF_16, renderData->indices);
		}
		else
		{
			RenderManager::Instance()->HWDrawElements(PRIMITIVETYPE_TRIANGLELIST, indexCount, EIF_16, indices);
		}
    }
}
    
void NMaterial::SetParent(NMaterial* material)
{
	if(state.parent)
	{
		state.parent->RemoveChild(this);
	}
	
	ResetParent();
	state.parent = SafeRetain(material);
	
	if(state.parent)
	{
		state.parent->AddChild(this);
	}
}
	
void NMaterial::AddChild(NMaterial* material)
{
	DVASSERT(material);
	DVASSERT(material != this);
	//sanity check if such material is already present
	DVASSERT(std::find(children.begin(), children.end(), material) == children.end());
	
	if(material)
	{
		SafeRetain(material);
		children.push_back(material);
		
		//TODO: propagate properties such as defines, textures, render state etc here
		material->OnParentChanged();
	}
}
	
void NMaterial::RemoveChild(NMaterial* material)
{
	DVASSERT(material);
	DVASSERT(material != this);
	
	Vector<NMaterial*>::iterator child = std::find(children.begin(), children.end(), material);
	if(children.end() != child)
	{
		material->ResetParent();
		children.erase(child);
	}
}
	
void NMaterial::ResetParent()
{
	//TODO: clear parent states such as textures, defines, etc
	effectiveLayers.Clear();
	effectiveLayers.Combine(state.layers);
	UnPropagateParentDefines();
	
	SafeRelease(state.parent);
}
	
void NMaterial::PropagateParentDefines()
{
	ready = false;
	inheritedDefines.Clear();
	if(state.parent)
	{
		if(state.parent->inheritedDefines.Size() > 0)
		{
			inheritedDefines.Combine(state.parent->inheritedDefines);
		}
		
		if(state.parent->state.nativeDefines.Size() > 0)
		{
			inheritedDefines.Combine(state.parent->state.nativeDefines);
		}
	}	
}

void NMaterial::UnPropagateParentDefines()
{
	ready = false;
	inheritedDefines.Clear();
}
	
void NMaterial::Rebuild(bool recursive)
{
	FastNameSet combinedDefines = inheritedDefines;
	
	if(state.nativeDefines.Size() > 0)
	{
		combinedDefines.Combine(state.nativeDefines);
	}
	
	HashMap<FastName, MaterialTechnique *>::Iterator iter = state.techniqueForRenderPass.Begin();
	while(iter != state.techniqueForRenderPass.End())
	{
		MaterialTechnique* technique = iter.GetValue();
		technique->RecompileShader(combinedDefines);
		
		Shader* shader = technique->GetShader();
		
		//VI: assume that uniform "lightPosition0" indicates vertex or pixel lighting
		int lightPositionUniformIndex = shader->FindUniformIndexByName("lightPosition0");
		materialDynamicLit = (lightPositionUniformIndex >= 0);		
		++iter;
	}
	
	if(recursive)
	{
		size_t childrenCount = children.size();
		for(size_t i = 0; i < childrenCount; ++i)
		{
			children[i]->Rebuild(recursive);
		}
	}
	
	ready = true;
}
	
void NMaterial::AddMaterialDefine(const FastName& defineName)
{
	ready = false;
	state.nativeDefines.Insert(defineName);
	
	NotifyChildrenOnChange();
}
	
void NMaterial::RemoveMaterialDefine(const FastName& defineName)
{
	ready = false;
	state.nativeDefines.Remove(defineName);
	
	NotifyChildrenOnChange();
}
	
const FastNameSet& NMaterial::GetRenderLayers()
{
	return effectiveLayers;
}
	
void NMaterial::OnParentChanged()
{
	PropagateParentLayers();
	PropagateParentDefines();
	
	NotifyChildrenOnChange();
}
	
void NMaterial::NotifyChildrenOnChange()
{
	size_t count = children.size();
	for(size_t i = 0; i < count; ++i)
	{
		children[i]->OnParentChanged();
	}
}
	
void NMaterial::PropagateParentLayers()
{
	effectiveLayers.Clear();
	effectiveLayers.Combine(state.layers);
	
	if(state.parent)
	{
		const FastNameSet& parentLayers = state.parent->GetRenderLayers();
		effectiveLayers.Combine(parentLayers);
	}
}
	
NMaterial* NMaterial::CreateChild()
{
	NMaterial* childMaterial = new NMaterial();
	
	HashMap<FastName, MaterialTechnique *>::Iterator iter = state.techniqueForRenderPass.Begin();
	while(iter != state.techniqueForRenderPass.End())
	{
		MaterialTechnique* technique = iter.GetValue();
		
		RenderState* newRenderState = new RenderState();
		technique->GetRenderState()->CopyTo(newRenderState);
		MaterialTechnique* childMaterialTechnique = new MaterialTechnique(technique->GetShaderName(),
																		  technique->GetUniqueDefineSet(),
																		  newRenderState);
		childMaterial->AddMaterialTechnique(iter.GetKey(), childMaterialTechnique);
		
		++iter;
	}

	char uniqueName[16];
	snprintf(uniqueName, 16, ".%lu", children.size());
	String childName = this->GetName() + uniqueName;
	
	childMaterial->SetName(childName);
	childMaterial->SetMaterialName(childName);
	childMaterial->SetParent(this);
	return childMaterial;
}
	
void NMaterial::SetupPerFrameProperties(Camera* camera)
{
	//VI: this is vertex or pixel lit material
	//VI: setup light for the material
	//VI: TODO: deal with multiple lights
	if(camera && materialDynamicLit && lights[0])
	{
		NMaterialProperty* propAmbientColor = GetMaterialProperty("prop_ambientColor");
		NMaterialProperty* propDiffuseColor = GetMaterialProperty("prop_diffuseColor");
		NMaterialProperty* propSpecularColor = GetMaterialProperty("prop_specularColor");
		
		const Matrix4 & matrix = camera->GetMatrix();
		Vector3 lightPosition0InCameraSpace = lights[0]->GetPosition() * matrix;
		Color materialAmbientColor = (propAmbientColor) ? *(Color*)propAmbientColor->data : Color(1, 1, 1, 1);
		Color materialDiffuseColor = (propDiffuseColor) ? *(Color*)propDiffuseColor->data : Color(1, 1, 1, 1);
		Color materialSpecularColor = (propSpecularColor) ? *(Color*)propSpecularColor->data : Color(1, 1, 1, 1);
		float32 intensity = lights[0]->GetIntensity();
		
		materialAmbientColor = materialAmbientColor * lights[0]->GetAmbientColor();
		materialDiffuseColor = materialDiffuseColor * lights[0]->GetDiffuseColor();
		materialSpecularColor = materialSpecularColor * lights[0]->GetSpecularColor();
		
		SetPropertyValue("lightPosition0", Shader::UT_FLOAT_VEC3, 1, lightPosition0InCameraSpace.data);
		
		SetPropertyValue("materialLightAmbientColor", Shader::UT_FLOAT_VEC3, 1, &materialAmbientColor);
		SetPropertyValue("materialLightDiffuseColor", Shader::UT_FLOAT_VEC3, 1, &materialDiffuseColor);
		SetPropertyValue("materialLightSpecularColor", Shader::UT_FLOAT_VEC3, 1, &materialSpecularColor);
		SetPropertyValue("lightIntensity0", Shader::UT_FLOAT, 1, &intensity);
	}
}
	
void NMaterial::Save(KeyedArchive * archive, SerializationContext * serializationContext)
{
	//VI: TODO: serialize multiple states per single LOD material
	Serialize(state, archive, serializationContext);
}
	
void NMaterial::Load(KeyedArchive * archive, SerializationContext * serializationContext)
{
	//VI: TODO: deserialize multiple states per single LOD material
	Deserialize(state, archive, serializationContext);
}
	
void NMaterial::Serialize(const MaterialState& materialState,
						  KeyedArchive * archive,
						  SerializationContext * serializationContext)
{
	archive->SetString("materialName", materialState.materialName.c_str());
	archive->SetString("parentName", (materialState.parent) ? materialState.parent->state.materialName.c_str() : "");
	
	KeyedArchive* materialLayers = new KeyedArchive();
	SerializeFastNameSet(materialState.layers, materialLayers);
	archive->SetArchive("layers", materialLayers);
	SafeRelease(materialLayers);
	
	
	KeyedArchive* materialNativeDefines = new KeyedArchive();
	SerializeFastNameSet(materialState.nativeDefines, materialNativeDefines);
	archive->SetArchive("nativeDefines", materialNativeDefines);
	SafeRelease(materialNativeDefines);

	KeyedArchive* materialProps = new KeyedArchive();
	for(HashMap<FastName, NMaterialProperty*>::Iterator it = materialState.materialProperties.Begin();
		it != materialState.materialProperties.End();
		++it)
	{
		NMaterialProperty* property = it.GetValue();
		
		uint32 propDataSize = Shader::GetUniformTypeSize(property->type) * property->size;
		uint8* propertyStorage = new uint8[propDataSize + sizeof(uint32) + sizeof(uint32)];
		
		uint32 uniformType = property->type; //make sure uniform type is always uint32
		memcpy(propertyStorage, &uniformType, sizeof(uint32));
		memcpy(propertyStorage + sizeof(uint32), &property->size, sizeof(uint32));
		memcpy(propertyStorage + sizeof(uint32) + sizeof(uint32), property->data, propDataSize);
		
		materialProps->SetByteArray(it.GetKey().c_str(), propertyStorage, propDataSize + sizeof(uint32) + sizeof(uint32));
	}
	archive->SetArchive("properties", materialProps);
	SafeRelease(materialProps);
	
	KeyedArchive* materialTextures = new KeyedArchive();
	for(HashMap<FastName, TextureBucket*>::Iterator it = materialState.textures.Begin();
		it != materialState.textures.End();
		++it)
	{
		materialTextures->SetString(it.GetKey().c_str(), it.GetValue()->texture->GetPathname().GetRelativePathname(serializationContext->GetScenePath()));
	}
	archive->SetArchive("textures", materialTextures);
	SafeRelease(materialTextures);
	
	int techniqueIndex = 0;
	KeyedArchive* materialTechniques = new KeyedArchive();
	for(HashMap<FastName, MaterialTechnique *>::Iterator it = state.techniqueForRenderPass.Begin();
		it != state.techniqueForRenderPass.End();
		++it)
	{
		MaterialTechnique* technique = it.GetValue();
		KeyedArchive* techniqueArchive = new KeyedArchive();
		
		techniqueArchive->SetString("renderPass", it.GetKey().c_str());
		techniqueArchive->SetString("shaderName", it.GetValue()->GetShaderName().c_str());
		
		KeyedArchive* techniqueDefines = new KeyedArchive();
		const FastNameSet& techniqueDefinesSet = technique->GetUniqueDefineSet();
		SerializeFastNameSet(techniqueDefinesSet, techniqueDefines);
		techniqueArchive->SetArchive("defines", techniqueDefines);
		SafeRelease(techniqueDefines);
		
		KeyedArchive* techniqueRenderState = new KeyedArchive();
		RenderState* renderState = technique->GetRenderState();
		renderState->Serialize(techniqueRenderState, serializationContext);
		techniqueArchive->SetArchive("renderState", techniqueRenderState);
		SafeRelease(techniqueRenderState);
		
		materialTechniques->SetArchive(Format("technique.%d", techniqueIndex), techniqueArchive);
		SafeRelease(techniqueArchive);
		techniqueIndex++;
	}
	archive->SetArchive("techniques", materialTechniques);
	SafeRelease(materialTechniques);
}
	
void NMaterial::Deserialize(MaterialState& materialState,
							KeyedArchive * archive,
							SerializationContext * serializationContext)
{
	String parentName = archive->GetString("parentName");
	
	materialState.materialName = archive->GetString("materialName");
	
	DeserializeFastNameSet(archive->GetArchive("layers"), materialState.layers);
	DeserializeFastNameSet(archive->GetArchive("nativeDefines"), materialState.nativeDefines);
	
	const Map<String, VariantType*>& propsMap = archive->GetArchive("properties")->GetArchieveData();
	for(Map<String, VariantType*>::const_iterator it = propsMap.begin();
		it != propsMap.end();
		++it)
	{
		const VariantType* propVariant = it->second;
		DVASSERT(VariantType::TYPE_BYTE_ARRAY == propVariant->type);
		DVASSERT(propVariant->AsByteArraySize() >= (sizeof(uint32) + sizeof(uint32)));
		
		const uint8* ptr = propVariant->AsByteArray();
		
		SetPropertyValue(it->first, (Shader::eUniformType)*(const uint32*)ptr, *(((const uint32*)ptr) + 1), ptr + sizeof(uint32) + sizeof(uint32));
	}
	
	const Map<String, VariantType*>& texturesMap = archive->GetArchive("textures")->GetArchieveData();
	for(Map<String, VariantType*>::const_iterator it = texturesMap.begin();
		it != texturesMap.end();
		++it)
	{
		Texture* tex = Texture::CreateFromFile(it->second->AsString());
		SetTexture(it->first, tex);
	}
	
	//TODO: continue loading...
}
	
void NMaterial::DeserializeFastNameSet(const KeyedArchive* srcArchive, FastNameSet& targetSet)
{
	const Map<String, VariantType*>& setData = srcArchive->GetArchieveData();
	for(Map<String, VariantType*>::const_iterator it = setData.begin();
		it != setData.end();
		++it)
	{
		targetSet.Insert(it->first);
	}
}
	
void NMaterial::SerializeFastNameSet(const FastNameSet& srcSet, KeyedArchive* targetArchive)
{
	for(FastNameSet::Iterator it = srcSet.Begin();
		it != srcSet.End();
		++it)
	{
		targetArchive->SetBool(it.GetKey().c_str(), true);
	}
}
	
};
