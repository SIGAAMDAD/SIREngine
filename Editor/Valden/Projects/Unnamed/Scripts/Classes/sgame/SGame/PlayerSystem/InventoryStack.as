namespace TheNomad::SGame {
    class InventoryStack {
        InventoryStack() {
        }
        InventoryStack( InfoSystem::ItemInfo@ type ) {
            Clear();
            @m_Type = @type;
            m_StackList.Reserve( m_Type.maxStackSize );
        }

        void PushItem( EntityObject@ item ) {
            if ( @item.GetInfo() !is cast<InfoSystem::InfoLoader@>( @m_Type ) ) {
                ConsolePrint( "InventoryStack::PushItem: item doesn't have the same type as this slot\n" );
                return;
            }
            if ( m_StackList.Count() == m_Type.maxStackSize ) {
                return;
            }
            m_StackList.Add( @item );
        }
        EntityObject@ PopItem() {
            EntityObject@ item = @m_StackList[ m_StackList.Count() - 1 ];
            m_StackList.RemoveLast();
            return @item;
        }
        array<EntityObject@>@ GetItemList( uint nItems ) {
            array<EntityObject@> items;

            items.Reserve( nItems );
            for ( uint i = 0; i < nItems; i++ ) {
                @items[ i ] = @m_StackList[i];
                m_StackList.RemoveAt( i );
            }

            return @items;
        }
        array<EntityObject@>@ GetItemListFrom( uint nFirstElem, uint nItems ) {
            array<EntityObject@> items;

            if ( nFirstElem + nItems >= m_StackList.Count() ) {
                DebugPrint( "InventoryStack::GetItemListFrom: out of range\n" );
            }

            items.Reserve( nFirstElem + nItems );
            for ( uint i = 0; i < nItems; i++ ) {
                if ( nFirstElem + i >= m_StackList.Count() ) {
                    break; // dont go over
                }
                items.Add( @m_StackList[ nFirstElem + i ] );
                m_StackList.RemoveAt( nFirstElem + i );
            }

            return @items;
        }

        InfoSystem::ItemInfo@ GetItemType() {
            return @m_Type;
        }
        const InfoSystem::ItemInfo@ GetItemType() const {
            return @m_Type;
        }

        void Clear() {
            @m_Type = null;
            m_StackList.Clear();
        }
        uint Count() const {
            return m_StackList.Count();
        }

        protected array<EntityObject@> m_StackList;
        protected InfoSystem::ItemInfo@ m_Type = null;
    };
};