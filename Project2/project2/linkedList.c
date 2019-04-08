#include <stdio.h>
#include <stdlib.h>

struct node
{
	int data;
	struct node* next;
};

struct node* head = NULL;


void buildNode( struct node* n_node, int data)
{
	n_node->data = data;
	n_node->next = NULL;
}

void addNode( struct node* ptr,  int data)
{
	ptr->next = NULL;
	ptr->data = data;

	struct node* temp = head;

	while (temp->next != NULL)
	{
		temp = temp->next;
	}


	temp->next = ptr;

}

void add( int data)
{
	if ( head == NULL)
	{
		struct node* newNode = malloc(sizeof(newNode));
		buildNode(newNode, data);
		head = newNode;
	}
	else {
		struct node* newNode = malloc(sizeof(newNode));

		addNode(newNode, data);
	}
}

int getNext()
{
	int value;
	if ( head != NULL)
	{
		value = head->data;
		struct node* temp = head;
		head = temp->next;
		free(temp);
	}
	else
	{
		value = -1;
	}

	return value;
}

int isEmpty()
{
	if ( head == NULL)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

void printall()
{
	struct node* ptr;

	ptr = head;

	while( ptr != NULL)
	{
		printf( "data: %d ", ptr->data);
		ptr = ptr->next;
	}
	printf("\n");
}
