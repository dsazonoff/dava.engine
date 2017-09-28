#include "Modules/CreatingControlsModule/CreatingControlsSystem.h"
#include "Modules/DocumentsModule/DocumentData.h"
#include "Modules/ProjectModule/ProjectData.h"
#include "UI/CommandExecutor.h"
#include "Utils/ControlPlacementUtils.h"

#include <Functional/Functional.h>

CreatingControlsSystem::CreatingControlsSystem(DAVA::TArc::ContextAccessor* accessor, DAVA::TArc::UI* ui)
    : BaseEditorSystem(accessor)
    , ui(ui)
{
    documentDataWrapper = accessor->CreateWrapper(DAVA::ReflectedTypeDB::Get<DocumentData>());
    BindFields();
}

void CreatingControlsSystem::BindFields()
{
    using namespace DAVA::TArc;

    fieldBinder.reset(new FieldBinder(accessor));
    {
        FieldDescriptor fieldDescr;
        fieldDescr.type = ReflectedTypeDB::Get<ProjectData>();
        fieldDescr.fieldName = FastName(ProjectData::projectPathPropertyName);
        fieldBinder->BindField(fieldDescr, DAVA::MakeFunction(this, &CreatingControlsSystem::OnProjectPathChanged));
    }
    {
        FieldDescriptor fieldDescr;
        fieldDescr.type = ReflectedTypeDB::Get<DocumentData>();
        fieldDescr.fieldName = FastName(DocumentData::packagePropertyName);
        fieldBinder->BindField(fieldDescr, DAVA::MakeFunction(this, &CreatingControlsSystem::OnPackageChanged));
    }
}

void CreatingControlsSystem::OnPackageChanged(const DAVA::Any& package)
{
    CancelCreateByClick();
}

void CreatingControlsSystem::OnProjectPathChanged(const DAVA::Any& projectPath)
{
    CancelCreateByClick();
}

EditorSystemsManager::eDragState CreatingControlsSystem::RequireNewState(DAVA::UIEvent* currentInput)
{
    return controlYamlString.empty() ? EditorSystemsManager::NoDrag : EditorSystemsManager::AddingControl;
}

bool CreatingControlsSystem::CanProcessInput(DAVA::UIEvent* currentInput) const
{
    return (GetSystemsManager()->GetDragState() == EditorSystemsManager::AddingControl);
}

void CreatingControlsSystem::ProcessInput(DAVA::UIEvent* currentInput)
{
    using namespace DAVA;

    if (currentInput->device == eInputDevices::MOUSE && currentInput->phase == UIEvent::Phase::ENDED && currentInput->mouseButton == eMouseButtons::LEFT)
    {
        AddControlAtPoint(currentInput->point);
    }
}

void CreatingControlsSystem::OnDragStateChanged(EditorSystemsManager::eDragState currentState, EditorSystemsManager::eDragState previousState)
{
    if (previousState == EditorSystemsManager::AddingControl)
    {
        CancelCreateByClick();
    }
}

BaseEditorSystem::eSystems CreatingControlsSystem::GetOrder() const
{
    return CREATING_CONTROLS;
}

void CreatingControlsSystem::SetCreateByClick(const DAVA::String& _controlYamlString)
{
    controlYamlString = _controlYamlString;
}

void CreatingControlsSystem::AddControlAtPoint(const DAVA::Vector2& point)
{
    DAVA::TArc::DataContext* active = accessor->GetActiveContext();
    DVASSERT(active != nullptr);
    DocumentData* docData = active->GetData<DocumentData>();
    if (docData != nullptr)
    {
        PackageNode* package = docData->GetPackageNode();
        DVASSERT(package != nullptr);

        uint32 destIndex = 0;
        PackageBaseNode* destNode = GetSystemsManager()->GetControlNodeAtPoint(point);
        if (destNode == nullptr)
        {
            destNode = DynamicTypeCheck<PackageBaseNode*>(package->GetPackageControlsNode());
            destIndex = GetSystemsManager()->GetIndexOfNearestRootControl(point);
        }
        else
        {
            destIndex = destNode->GetCount();
        }

        ControlsContainerNode* destControlContainer = dynamic_cast<ControlsContainerNode*>(destNode);
        if (destControlContainer != nullptr)
        {
            docData->BeginBatch("Copy control from library");
            CommandExecutor executor(accessor, ui);

            DAVA::Set<PackageBaseNode*> newNodes = executor.Paste(docData->GetPackageNode(), destNode, destIndex, controlYamlString);

            if (destNode != package->GetPackageControlsNode())
            {
                ControlNode* destControl = dynamic_cast<ControlNode*>(destNode);
                if (destControl != nullptr && newNodes.size() == 1)
                {
                    ControlNode* newNode = dynamic_cast<ControlNode*>(*(newNodes.begin()));
                    ControlPlacementUtils::SetAbsoulutePosToControlNode(package, newNode, destControl, point);
                    AbstractProperty* postionProperty = newNode->GetRootProperty()->FindPropertyByName("position");
                    AbstractProperty* sizeProperty = newNode->GetRootProperty()->FindPropertyByName("size");
                    newNode->GetRootProperty()->SetProperty(postionProperty, Any(newNode->GetControl()->GetPosition()));
                    newNode->GetRootProperty()->SetProperty(sizeProperty, Any(newNode->GetControl()->GetSize()));

                    SelectedNodes newSelection = { newNode };
                    documentDataWrapper.SetFieldValue(DocumentData::selectionPropertyName, newSelection);
                }
            }

            docData->EndBatch();
        }
    }

    CancelCreateByClick();
}

void CreatingControlsSystem::CancelCreateByClick()
{
    controlYamlString.clear();
}
