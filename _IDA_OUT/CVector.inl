/** 
 * @file CVector.inl
 *
 * @brief CVector.
 *   
 * CVector.
 *
 * (c) 2010 Glu Mobile.  All rights reserved.
 */

#include "CVector.h"
#include "SharedSettings.h"

#if defined( TCVector_ManagedCompilation )
#define ManagedCompilationInline
#else
#define ManagedCompilationInline inline
#endif



template< class T >
ManagedCompilationInline
CVector< T >::CVector( const CVector& v )
	: CClass( ClassId_CVector )
{
	m_elementData = NULL;
	m_elementCount = 0;
	Copy( v );
}


template< class T >
ManagedCompilationInline
CVector< T >::CVector( const CVector& v1, const CVector& v2 )
	: CClass( ClassId_CVector )
{
	m_elementData = NULL;
	m_elementCount = 0;
	Copy( v1 );
	AddAll( v2 );
}


template< class T >
ManagedCompilationInline
CVector< T >::CVector( const CVector& v1, const CVector& v2, MultiVectorOp op )
	: CClass( ClassId_CVector )
{
	m_elementData = NULL;
	m_elementCount = 0;
	Copy( v1 );
	if( op == MVO_Add )
		AddAll( v2 );
	else // assume Sub
		RemoveAll( v2 );
}


template< class T >
ManagedCompilationInline
void CVector< T >::Add( int32 index, const T& element )
{
	ASSERT( index >= 0 && index < m_elementCount + 1 );
	EnsureCapacity( m_elementCount + 1 );
	for( int32 i = m_elementCount - 1; i >= index; i-- )
		m_elementData[ i + 1 ] = m_elementData[ i ];
	m_elementData[ index ] = element;
	m_elementCount++;
}


template< class T >
ManagedCompilationInline
boolean CVector< T >::AddAll( const CVector& v )
{
	EnsureCapacity( m_elementCount + v.m_elementCount );
	for( int32 i = m_elementCount; i < m_elementCount + v.m_elementCount; i++ )
		m_elementData[ i ] = v.m_elementData[ i - m_elementCount ];
	m_elementCount += v.m_elementCount;
	return TRUE;
}


template< class T >
ManagedCompilationInline
boolean CVector< T >::AddAll( int32 index, const CVector& v )
{
	EnsureCapacity( m_elementCount + v.m_elementCount );
	for( int32 i = m_elementCount - 1; i >= index; i-- )
		m_elementData[ i + v.m_elementCount ] = m_elementData[ i ];
	for( int32 i = index; i < index + v.m_elementCount; i++ )
		m_elementData[ i ] = v.m_elementData[ i - index ];
	m_elementCount += v.m_elementCount;
	return TRUE;
}


template< class T >
ManagedCompilationInline
boolean CVector< T >::ContainsAll( const CVector& v ) const
{
	boolean success = FALSE;
	for( int32 i = 0; i < v.m_elementCount; i++ )
	{
		success = FALSE;
		for( int32 j = 0; j < m_elementCount; j++ )
		{
			if( v.m_elementData[ i ] == m_elementData[ j ] )
			{
				success = TRUE;
				break; 
			}
		}
		if( !success )
			break;
	}
	return success;
}


template< class T >
ManagedCompilationInline
void CVector< T >::CopyInto( T* elementArray, int32 numOfElementsToCopy, int32 startIndex )
{
	ASSERT( startIndex >= 0 && numOfElementsToCopy >= 0 );
	int32 i = startIndex;
	for( ; i < m_elementCount; i++ )
	{
		if( i - startIndex == numOfElementsToCopy )
			break;
		elementArray[ i - startIndex ] = m_elementData[ i ];
	}
	ASSERT( i - startIndex == numOfElementsToCopy ); // Copy was not completed
}


template< class T >
ManagedCompilationInline
void CVector< T >::EnsureCapacity( int32 minCapacity )
{
	if( m_capacity < minCapacity )
	{
		m_capacity += ( m_capacityIncrement > 0 )? m_capacityIncrement : m_capacity;
		if( m_capacity < minCapacity )
			m_capacity = minCapacity;
		ASSERT( m_capacity );
		T* elementData = new T[ m_capacity ];
		CopyInto( elementData, m_elementCount );
		if( m_elementData )
			delete[] m_elementData;
		m_elementData = elementData;
	}
}


template< class T >
ManagedCompilationInline
boolean CVector< T >::Remove( const T& element )
{
	for( int32 i = 0; i < m_elementCount; i++ )
	{
		if( element == m_elementData[ i ] )
		{
			CopyInto( m_elementData + i, m_elementCount - i - 1, i + 1 );
			m_elementCount--;
			return TRUE;
		}
	}
	return FALSE;
}


template< class T >
ManagedCompilationInline
boolean CVector< T >::RemoveAll( const CVector& v )
{
	boolean success = FALSE;
	for( int32 i = 0; i < v.m_elementCount; i++ )
	{
		for( int32 j = m_elementCount - 1; j >= 0; j-- )
		{
			if( v.m_elementData[ i ] == m_elementData[ j ] )
			{
				RemoveAt( j );
				success = TRUE;			
			}
		}
	}
	return success;
}


template< class T >
ManagedCompilationInline
boolean CVector< T >::RetainAll( const CVector& v )
{
	boolean success = FALSE;
	if( v.m_elementCount > 0 )
	{
		for( int32 i = m_elementCount - 1; i >= 0; i-- )
		{
			int32 j = 0;
			for( ; j < v.m_elementCount; j++ )
			{
				if( v.m_elementData[ j ] == m_elementData[ i ] )
					break;
			}
			if( j == v.m_elementCount )
			{
				RemoveAt( i );
				success = TRUE; 
			}
		}
	}
	else
	if( m_elementCount )
	{
		Clear();
		success = TRUE;
	}
	return success;
}


template< class T >
ManagedCompilationInline
void CVector< T >::TrimToSize()
{
	if( m_elementCount < m_capacity )
	{
		if( m_elementCount == 0 )
		{
			if( m_elementData )
				delete[] m_elementData;
			m_elementData = NULL;
			m_capacity = 0;
		}
		else
		{
			m_capacity = m_elementCount;
			T* elementData = new T[ m_capacity ];
			CopyInto( elementData, m_elementCount );
			delete[] m_elementData;
			m_elementData = elementData;
		}
	}
}


template< class T >
ManagedCompilationInline
void CVector< T >::Copy( const CVector& v )
{
	if( m_elementData )
		delete[] m_elementData;
	if( v.m_capacity )
	{
		m_elementData = new T[ v.m_capacity ];
		ASSERT( m_elementData );
		for( int32 i = 0; i < v.m_elementCount; i++ )
			m_elementData[ i ] = v.m_elementData[ i ];
	}
	else
		m_elementData = NULL;
	m_capacity = v.m_capacity;
	m_capacityIncrement = v.m_capacityIncrement;
	m_elementCount = v.m_elementCount;
}
