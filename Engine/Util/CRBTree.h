#ifndef __CRBTREE_H__
#define __CRBTREE_H__

#pragma once

template<typename T>
class CDefOps
{
public:
    static bool LessFunc( const T& lhs, const T& rhs )
    { return ( lhs < rhs ); }
};

template<typename IndexType>
struct CRBTreeLinks_t {
	IndexType  m_Left;
	IndexType  m_Right;
	IndexType  m_Parent;
	IndexType  m_Tag;
};

template<typename T, typename IndexType>
struct CRBTreeNode_t : public CRBTreeLinks_t<IndexType>

#endif