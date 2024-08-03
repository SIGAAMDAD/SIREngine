#ifndef __CLINKED_LIST_H__
#define __CLINKED_LIST_H__

#pragma once

template<typename T>
struct LinkedListNodeDoubleLinked {
    LinkedListNode *pNext;
    LinkedListNode *pPrev;
    T data;
};

template<typename T>
class CLinkedList
{
public:
    CLinkedList( void );
private:
    LinkedListNodeDoubleLinked<T> m_List;
};

class CSingleLinkList
{
public:
private:
};

#endif