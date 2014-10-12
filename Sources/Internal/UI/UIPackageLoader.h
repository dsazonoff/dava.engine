#ifndef __DAVAENGINE_UI_PACKAGE_LOADER_H__
#define __DAVAENGINE_UI_PACKAGE_LOADER_H__
#include "Base/BaseObject.h"
#include "Base/BaseTypes.h"
#include "Base/BaseMath.h"

#include "AbstractUIPackageBuilder.h"

namespace DAVA
{
class UIYamlLoader;
class UIControl;
class YamlNode;
class FilePath;
class UIPackage;
class UIControlFactory;
class UIControlBackground;
    
class UIPackageLoader
{
public:
    UIPackageLoader(AbstractUIPackageBuilder *builder);
    virtual ~UIPackageLoader();

public:
    UIPackage *LoadPackage(const FilePath &packagePath);
    bool LoadControlByName(const String &name);

private:
    void LoadControl(const YamlNode *node);

    void LoadControlPropertiesFromYamlNode(UIControl *control, const InspInfo *typeInfo, const YamlNode *node);
    void LoadBgPropertiesFromYamlNode(UIControl *control, const YamlNode *node);
    void LoadInternalControlPropertiesFromYamlNode(UIControl *control, const YamlNode *node);
    virtual VariantType ReadVariantTypeFromYamlNode(const InspMember *member, const YamlNode *node);

private:
    UIYamlLoader *yamlLoader;

    enum eItemStatus {
        STATUS_WAIT,
        STATUS_LOADING,
        STATUS_LOADED
    };
    
    struct QueueItem {
        String name;
        const YamlNode *node;
        int status;
    };
    Vector<QueueItem> loadingQueue;
    AbstractUIPackageBuilder *builder;
};

};

#endif // __DAVAENGINE_UI_PACKAGE_LOADER_H__
