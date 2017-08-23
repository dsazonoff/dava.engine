#pragma once

#include "Base/BaseObject.h"

#include "Math/Vector.h"
#include "Math/Matrix4.h"
#include "Particles/ParticlePropertyLine.h"

namespace DAVA
{
struct ParticleLayer;

class ParticleDragForce : public BaseObject
{
public:
    enum class eShape
    {
        BOX,
        SPHERE
    } shape = eShape::BOX;

    enum class eTimingType
    {
        CONSTANT,
        OVER_LAYER_LIFE,
        OVER_PARTICLE_LIFE
    } timingType = eTimingType::CONSTANT;

    enum class eType
    {
        DRAG_FORCE,
        LORENTZ_FORCE,
        POINT_GRAVITY,
        BOX_WRAP
    } type = eType::DRAG_FORCE;
    
    ParticleDragForce(ParticleLayer* parent);

    ParticleDragForce* Clone();

    bool isActive = true;
    String forceName = "DragForce";
    Matrix4 localMatrix;

    Vector3 position;
    Vector3 rotation;

    Vector3 boxSize{ 1.0f, 1.0f, 1.0f };
    Vector3 forcePower{ 1.0f, 1.0f, 1.0f };
    RefPtr<PropertyLine<Vector3>> forcePowerLine;
    float32 radius = 1.0f;

    bool isInfinityRange = true;
    void GetModifableLines(List<ModifiablePropertyLineBase*>& modifiables);

public:
    INTROSPECTION_EXTEND(ParticleDragForce, BaseObject, nullptr)

private:
    ParticleLayer* parentLayer = nullptr;
};
}
