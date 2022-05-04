#include "list.h"
#include <stdlib.h>
#include "fatal.h"

/* Place in the interface file */
struct Node
{
	ElementType	Element;
	Position		Next;
};

List MakeEmpty( List L )
{
	if( L != NULL )
		DeleteList( L ); /* L is not freed???? */
	L = malloc( sizeof( struct Node ) );
	if( L == NULL )
		FatalError( "Out of memory!" );
	L->Next = NULL;
	return L;
}

/* START: fig3_8.txt */
/* Return true if L is empty */

int IsEmpty( List L )
{
	return L->Next == NULL;
}
/* END */

/* START: fig3_9.txt */
/* Return true if P is the last position in list L */
/* Parameter L is unused in this implementation */
int IsLast( Position P, List L )
{
	return P->Next == NULL;
}

/* START: fig3_10.txt */
/* Return Position of X in L; NULL if not found */
Position Find( ElementType X, List L )
{
	Position P;

	P = L->Next;
	while( P != NULL && P->Element != X )
		P = P->Next;

	return P;
}


/* START: fig3_11.txt */
/* Delete from a list */
/* Cell pointed to by P->Next is wiped out */
/* Assume that the position is legal */
/* Assume use of a header node */

void Delete( ElementType X, List L )
{
	Position P, TmpCell;

	P = FindPrevious( X, L );

	if( !IsLast( P, L ) )  /* Assumption of header use */
	{/* X is found; delete it */
		TmpCell = P->Next;
		P->Next = TmpCell->Next;  /* Bypass deleted cell */
		free( TmpCell );
	}
	/* ?? when P is the last one?? */
}

/* START: fig3_12.txt */
/* If X is not found, then Next field of returned value is NULL */
Position FindPrevious( ElementType X, List L )
{
	Position P;

	P = L;
	while( P->Next != NULL && P->Next->Element != X )
		P = P->Next;

	return P;
}

/* START: fig3_13.txt */
/* Insert (after legal position P) */
/* Header implementation assumed */
/* Parameter L is unused in this implementation */
void Insert( ElementType X, List L, Position P )
{
	Position TmpCell;

	TmpCell = malloc( sizeof( struct Node ) );
	if( TmpCell == NULL )
		FatalError( "Out of space!!!" );

	TmpCell->Element = X;
	TmpCell->Next = P->Next;
	P->Next = TmpCell;
}

#if 0
/* START: fig3_14.txt */
/* Incorrect DeleteList algorithm */

void
DeleteList( List L )
{
    Position P;

/* 1*/      P = L->Next;  /* Header assumed */
/* 2*/      L->Next = NULL;
/* 3*/      while( P != NULL )
    {
/* 4*/          free( P );
/* 5*/          P = P->Next;
    }
}
/* END */
#endif

/* START: fig3_15.txt */
/* Correct DeleteList algorithm */
void DeleteList( List L )
{
	Position P, Tmp;

	P = L->Next;  /* Header assumed */
	L->Next = NULL;	/* header's next is null, eg. list is null */
	
	while( P != NULL )
	{
		Tmp = P->Next;
		free( P );
		P = Tmp;
	}
}


Position Header( List L )
{
	return L;
}

Position First( List L )
{
	return L->Next;
}

Position Advance( Position P )
{
	return P->Next;
}

ElementType Retrieve( Position P )
{
	return P->Element;
}

