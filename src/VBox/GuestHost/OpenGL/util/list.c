#include "cr_list.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct CRListIterator {
	void *element;
	CRListIterator *prev;
	CRListIterator *next;
};

struct CRList {
	CRListIterator *head;
	CRListIterator *tail;
	unsigned size;
};

CRList *crAllocList( void )
{
	CRList *l = malloc( sizeof( CRList ) );
	assert( l );

	l->head = malloc( sizeof( CRListIterator ) );
	assert( l->head );

	l->tail = malloc( sizeof( CRListIterator ) );
	assert( l->tail );

	l->head->prev = NULL;
	l->head->next = l->tail;

	l->tail->prev = l->head;
	l->tail->next = NULL;

	l->size = 0;

	return l;
}

void crFreeList( CRList *l )
{
	CRListIterator *t1;

	assert( l != NULL );
	t1 = l->head;
	while ( t1 != NULL )
	{
		CRListIterator *t2 = t1;
		t1 = t1->next;
		t2->prev = NULL;
		t2->next = NULL;
		t2->element = NULL;
		free( t2 );
	}
	l->size = 0;
	free( l );
}

unsigned crListSize( const CRList *l )
{
	return l->size;
}

int crListIsEmpty( const CRList *l )
{
	assert( l != NULL );
	return l->size == 0;
}

void crListInsert( CRList *l, CRListIterator *iter, void *elem )
{
	CRListIterator *p;

	assert( l != NULL );
	assert( iter != NULL );
	assert( iter != l->head );

	p = malloc( sizeof( CRListIterator ) );
	assert( p != NULL );
	p->prev = iter->prev;
	p->next = iter;
	p->prev->next = p;
	iter->prev = p;

	p->element = elem;
	l->size++;
}

void crListErase( CRList *l, CRListIterator *iter )
{
	assert( l != NULL );
	assert( iter != NULL );
	assert( iter != l->head );
	assert( iter != l->tail );
	assert( l->size > 0 );

	iter->next->prev = iter->prev;
	iter->prev->next = iter->next;

	iter->prev = NULL;
	iter->next = NULL;
	iter->element = NULL;
	free( iter );

	l->size--;
}

void crListClear( CRList *l )
{
	assert( l != NULL );
	while ( !crListIsEmpty( l ) )
	{
		crListPopFront( l );
	}
}

void crListPushBack( CRList *l, void *elem )
{
	assert( l != NULL );
	crListInsert( l, l->tail, elem );
}

void crListPushFront( CRList *l, void *elem )
{
	assert( l != NULL );
	crListInsert( l, l->head->next, elem );
}

void crListPopBack( CRList *l )
{
	assert( l != NULL );
	assert( l->size > 0 );
	crListErase( l, l->tail->prev );
}

void crListPopFront( CRList *l )
{
	assert( l != NULL );
	assert( l->size > 0 );
	crListErase( l, l->head->next );
}

void *crListFront( CRList *l )
{
	assert( l != NULL );
	assert( l->size > 0 );
	assert( l->head != NULL );
	assert( l->head->next != NULL );
	return l->head->next->element;
}

void *crListBack( CRList *l )
{
	assert( l != NULL );
	assert( l->size > 0 );
	assert( l->tail != NULL );
	assert( l->tail->prev != NULL );
	return l->tail->prev->element;
}

CRListIterator *crListBegin( CRList *l )
{
	assert( l != NULL );
	assert( l->head != NULL );
	assert( l->head->next != NULL );
	return l->head->next;
}

CRListIterator *crListEnd( CRList *l )
{
	assert( l != NULL );
	assert( l->tail != NULL );
	return l->tail;
}

CRListIterator *crListNext( CRListIterator *iter )
{
	assert( iter != NULL );
	assert( iter->next != NULL );
	return iter->next;
}

CRListIterator *crListPrev( CRListIterator *iter )
{
	assert( iter != NULL );
	assert( iter->prev != NULL );
	return iter->prev;
}

void *crListElement( CRListIterator *iter )
{
	assert( iter != NULL );
	return iter->element;
}

CRListIterator *crListFind( CRList *l, void *element, CRListCompareFunc compare )
{
	CRListIterator *iter;

	assert( l != NULL );
	assert( compare );

	for ( iter = crListBegin( l ); iter != crListEnd( l ); iter = crListNext( iter ) )
	{
		if ( compare( element, iter->element ) == 0 )
		{
			return iter;
		}
	}
	return NULL;
}

void crListApply( CRList *l, CRListApplyFunc apply, void *arg )
{
	CRListIterator *iter;

	assert( l != NULL );
	for ( iter = crListBegin( l ); iter != crListEnd( l ); iter = crListNext( iter ) )
	{
		apply( iter->element, arg );
	}
}

#if CR_TESTING_LIST

static void printElement( void *elem, void *arg )
{
	char *s = elem;
	FILE *fp = arg;

	assert( s != NULL );
	assert( fp != NULL );
	fprintf( fp, "%s ", s );
}

static void printList( CRList *l )
{
	assert( l != NULL );
	crListApply( l, printElement, stderr );
	fprintf( stderr, "\n" );
}

static int elementCompare( void *a, void *b )
{
	return strcmp( a, b );
}

int main( void )
{
	char *names[] = { "a", "b", "c", "d", "e", "f" };
	CRList *l;
	CRListIterator *iter;
	int i, n;

	n = sizeof( names ) / sizeof( names[0] );
	fprintf( stderr, "n=%d\n", n );

	l = crAllocList(  );
	for ( i = 0; i < n; ++i )
	{
		crListPushBack( l, names[i] );
	}
	printList( l );

	crListPushFront( l, "x" );
	printList( l );

	crListPushBack( l, "y" );
	printList( l );

	crListPopFront( l );
	printList( l );

	crListPopBack( l );
	printList( l );

	iter = crListFind( l, "c", elementCompare );
	assert( iter != NULL );
	crListInsert( l, iter, "z" );
	printList( l );

	iter = crListFind( l, "d", elementCompare );
	assert( iter != NULL );
	crListErase( l, iter );
	printList( l );

	crListClear( l );
	printList( l );
	fprintf( stderr, "size: %d\n", crListSize( l ) );
	fprintf( stderr, "is empty: %d\n", crListIsEmpty( l ) );

	crListPushBack( l, "w" );
	crListPushBack( l, "t" );
	crListPushBack( l, "c" );
	printList( l );

	fprintf( stderr, "front: %s\n", ( char * ) crListFront( l ) );
	fprintf( stderr, "back: %s\n", ( char * ) crListBack( l ) );
	fprintf( stderr, "size: %d\n", crListSize( l ) );
	fprintf( stderr, "is empty: %d\n", crListIsEmpty( l ) );

	crFreeList( l );
	return 0;
}

#endif // CR_TESTING_LIST
