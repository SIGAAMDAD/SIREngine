#include "SGame/PlayerSystem/InventoryStack.as"

namespace TheNomad::SGame {
    const uint MAX_INVENTORY_SLOTS = 50;

    class InventoryManager {
        InventoryManager() {
        }

        private InventoryStack@ GetFreeStackSlot( InfoSystem::ItemInfo@ info ) {
            for ( uint i = 0; i < m_SlotList.Count(); i++ ) {
                if ( @m_SlotList[i] is null ) {
                    @m_SlotList[i] = InventoryStack( @info );
                    return @m_SlotList[i];
                }
            }
            return null;
        }

        InventoryStack@ GetItemStack( InfoSystem::ItemInfo@ info ) {
            for ( uint i = 0; i < m_SlotList.Count(); i++ ) {
                if ( @m_SlotList[i] is null ) {
                    continue;
                }
                if ( @m_SlotList[i].GetItemType() is @info ) {
                    return @m_SlotList[i];
                }
            }
            return null;
        }
        void AddItem( ItemObject@ item ) {
            InventoryStack@ stack = null;
            InfoSystem::ItemInfo@ itemType = @item.GetItemInfo();

            DebugPrint( "Pushing item of type " + itemType.type + " to inventory stack\n" );

            for ( uint i = 0; i < m_SlotList.Count(); i++ ) {
                if ( @m_SlotList[i] !is null && @itemType is @m_SlotList[i].GetItemType() ) {
                    @stack = @m_SlotList[i];
                    break;
                }
            }
            if ( @stack is null ) {
                @stack = GetFreeStackSlot( @itemType );
                if ( @stack is null ) {
                    return; // no free slots
                }
            }

            stack.PushItem( @item );
        }

        array<InventoryStack@>@ GetSlots() {
            return @m_SlotList;
        }
        const array<InventoryStack@>@ GetSlots() const {
            return @m_SlotList;
        }

        private array<InventoryStack@> m_SlotList( MAX_INVENTORY_SLOTS );
    };
};