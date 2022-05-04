/*
Input: 1->2->3->4->5
Output: 1->5->2->4->3

Input: 1->2->3->4->5->6
Output: 1->6->2->5->3->4

*/

// C++ program to sandwich the last part of
// linked list in between the first part of
// the linked list
#include<bits/stdc++.h>
#include <stdio.h>
using namespace std;

struct node {
	int data;
	struct node* next;
};

// Function to reverse Linked List
struct node* reverseLL(struct node* root)
{
	struct node *prev = NULL, *current = root, *next;
	while (current) {
		next = current->next;
		current->next = prev;
		prev = current;
		current = next;
	}

	// root needs to be updated after reversing
	root = prev;
	return root;
}

// Function to modify Linked List
void modifyLL(struct node* root)
{
	// Find the mid node
	struct node *slow_ptr = root, *fast_ptr = root;
	while (fast_ptr && fast_ptr->next) {
		fast_ptr = fast_ptr->next->next;
		slow_ptr = slow_ptr->next;
	}

	// Reverse the second half of the list
	struct node* root2 = reverseLL(slow_ptr->next);

	// partition the list
	slow_ptr->next = NULL;

	struct node *current1 = root, *current2 = root2;

	// insert the elements in between
	while (current1 && current2) {

		// next node to be traversed in the first list
		struct node* dnext1 = current1->next;

		// next node to be traversed in the first list
		struct node* dnext2 = current2->next;
		current1->next = current2;
		current2->next = dnext1;
		current1 = dnext1;
		current2 = dnext2;
	}
}

// Function to insert node after the end
void insertNode(struct node** start, int val)
{

	// allocate memory
	struct node* temp = (struct node*)malloc(sizeof(struct node));
	temp->data = val;

	// if first node
	if (*start == NULL)
		*start = temp;
	else {

		// move to the end pointer of node
		struct node* dstart = *start;
		while (dstart->next != NULL)
			dstart = dstart->next;
		dstart->next = temp;
	}
}

// function to print the linked list
void display(struct node** start)
{
	struct node* temp = *start;

	// traverse till the entire linked
	// list is printed
	while (temp->next != NULL) {
		printf("%d->", temp->data);
		temp = temp->next;
	}
	printf("%d\n", temp->data);
}

// Driver Code
int main()
{
	// Odd Length Linked List
	struct node* start = NULL;
	insertNode(&start, 1);
	insertNode(&start, 2);
	insertNode(&start, 3);
	insertNode(&start, 4);
	insertNode(&start, 5);

	printf("Before Modifying: ");
	display(&start);
	modifyLL(start);
	printf("After Modifying: ");
	display(&start);

	// Even Length Linked List
	start = NULL;
	insertNode(&start, 1);
	insertNode(&start, 2);
	insertNode(&start, 3);
	insertNode(&start, 4);
	insertNode(&start, 5);
	insertNode(&start, 6);

	printf("\nBefore Modifying: ");
	display(&start);
	modifyLL(start);
	printf("After Modifying: ");
	display(&start);

	return 0;
}

