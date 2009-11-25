// For conditions of distribution and use, see copyright notice in license.txt

/**
 *  @file InventoryEvents.h
 *  @brief Inventory events.
 */

#ifndef incl_Protocol_InventoryEvents_h
#define incl_Protocol_InventoryEvents_h

#include <QStringList>
#include <QVector>

namespace Inventory
{
    namespace Events
    {
        static const Core::event_id_t EVENT_INVENTORY_DESCENDENT = 0x01;
        static const Core::event_id_t EVENT_INVENTORY_UPLOAD = 0x02;
        static const Core::event_id_t EVENT_INVENTORY_UPLOAD_BUFFER = 0x03;
    }

    enum ItemType
    {
        IIT_Folder = 0,
        IIT_Asset
    };

    class InventoryItemEventData : public Foundation::EventDataInterface
    {
    public:
        InventoryItemEventData(const ItemType &type) : item_type(type) {}
        virtual ~InventoryItemEventData() {}
        ItemType item_type;
        RexUUID id;
        RexUUID assetId;
        RexUUID parentId;
        inventory_type_t inventoryType;
        asset_type_t assetType;
        std::string name;
        std::string description;
        //! Original filename of upload, if applicable
        std::string fileName;
    };

    class InventoryUploadEventData : public Foundation::EventDataInterface
    {
    public:
        InventoryUploadEventData() {}
        virtual ~InventoryUploadEventData() {}
        QStringList filenames;
    };
    
    class InventoryUploadBufferEventData : public Foundation::EventDataInterface
    {
    public:
        InventoryUploadBufferEventData() {}
        virtual ~InventoryUploadBufferEventData() {}
        QStringList filenames;
        QVector<QVector<Core::u8> > buffers;
    };
}

#endif // incl_Protocol_InventoryEvents_h