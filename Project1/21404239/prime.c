#include <stdio.h>
#include <stdlib.h>
#include  <sys/types.h>
#include  <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#define READ_END 0
#define WRITE_END 

typedef struct node {
    int val;
    struct node * next;
} myList;

void makeList(int i , myList **value , myList **tail);
void viewList(myList *value);
void removeItem(int deletingItem , myList **value);
int getSizeOfList(myList **head);
void removeFirst(myList **value);
void removeLast(myList **value);
void removeMiddle (myList **value ,int item);
bool checkPrime(int item);


int main(int argc, char const *argv[])
{

    if (argc != 3)
    {
        perror("Wrong Format");
        exit(0);
    }
    int number_process = atoi(argv[2]);
    int n = atoi(argv[1]);
    int someNumber = 0;
    if(n > 1000000 )
    {
        printf("too many prcesses\n");
        exit(0);
    }
    if(n < 1000)
    {
        printf("too less processes\n");
        exit(0);
    }
    if(number_process > 50 || number_process < 1)
    {
        printf("Incorrect number of fork\n");
        exit(0);
    }

    int prime = 0;
    myList *head = NULL;
    myList *tail = NULL;

    makeList(n, &head , &tail);
    //viewList(head);
    printf("the end of the list item in MAIN is %i\n", tail->val);



    int fd[number_process][2];
    int i =0 ;
    int j = 0;

    //created sll the pipes
    for(i = 0 ; i < number_process+1 ; i++)
    {
        if(pipe(fd[i]) == -1)
        {
            perror("pipe error");
            return 0;
        }
    }

    int writeFD[2];
    pipe(writeFD);
   
    myList *current  = head;
    int checkingValue = current->val;


    for( i = 0 ; i < number_process ; i++)
    {

        pid_t pid = fork();

        if(pid < 0)
        {
            exit(1);
        }

        if(pid == 0)
        {

            //wait(NULL);
            close(fd[i][1]); // close
            close(fd[i+1][0]);
            while(current->val != 1)
            {
               if(read(fd[i][0] , &prime , sizeof(int)) == -1)
               {
                   printf("Error reading from pipe in child\n");

               }
               else
               {
                    int checker = 0;
                    checker = checkPrime(prime);
                    if(checker == 1)
                    {
                        //WRTING THE PIPE OVER THE SEND TO MP
                        close(writeFD[0]);
                        write(writeFD[1] , &prime , sizeof(int));
                        printf("%i\n", prime);

                    }
               }
               if(write(fd[i+1][1] , &prime , sizeof(int)) == -1)
                {
                    printf("Error Writing to pipe in CHILD\n");  // write to the write end of the pipe
                }
                else
                {
                    //printf("Prime %i sent from child to main\n ", prime);
                }
               current = current->next;
           }



       }
       else
       {
            while(current->val != 1)
            {
                // To make it unblocking
                fcntl(fd[i][1], F_SETFL,O_NONBLOCK);
                checkingValue = current->val;
                //printf("Passing value in main %i\n", current->val );
                if(write(fd[i][1] , &checkingValue, sizeof(checkingValue))  == -1)
                {
                    printf("Error Writing %i from main to child\n" , checkingValue);  // write to the write end of the pipe
                }
                else
                {
                }


                current = current->next;

                if(read(fd[i+1][0] ,&checkingValue , sizeof(int)) == -1)
                {
                    printf("Error reading from pipe in main\n");
                }
                else{
                }
                
                //READING FROM THE CHILD TO THE READ AT the MP
                pid_t PRpID = fork();
                if (PRpID == 0)
                {

                    close(writeFD[1]);
                    //read(writeFD[0] , &someNumber , sizeof(int));
                    //printf("%i\n" , someNumber);
                    exit(0);
                }
            }
       }
   }


    for (i = 0; i < number_process; i++) {
        wait(NULL);
    }
    return 0;
}

bool checkPrime(int item)
{
    bool prime = true;
    if(item == 1)
    {
        prime = false;
        return prime;
    }
    for(int i = 1 ; i <= item ; i++)
    {
        if( (i != item) && (i != 1) && (item % i == 0))
        {
            prime = false;
        }

    }
    return prime;

}
void makeList(int limit , myList **value , myList **tail)
    {
        myList *head = NULL;
        myList *endOFList = NULL;
        for(int i = 2 ; i < limit ; i++)
        {
            if (head == NULL)
            {
                head = malloc(sizeof(myList));
                head->val = i;
                head->next = NULL;
            }
            else
            {
                myList *current = head;
                while(current->next != NULL)
                {
                    current = current->next;
                }
                current->next = malloc(sizeof(myList));
                current->next->val = i;
                current->next->next = NULL;
            }

        }

        myList *current = head;
        while(current->next != NULL)
        {
           current = current->next;
       }
       current->next = malloc(sizeof(myList));
       current->next->val = 1;
       current->next->next = NULL;
       endOFList = current->next;

       *tail = endOFList;
       printf("the end of the list item is %i\n", endOFList->val);

       *value = head;
       printf("List Created!\n");
   }

   void viewList(myList *value)
   {
    if(value == NULL)
    {
        printf("EmptyList\n");
        return;
    }
    printf("%s\n", "In View List function");
    myList *current = value;
    while(current != NULL)
    {
        printf("%i\n", current->val );
        current = current->next;
    }
}

void removeItem(int deletingItem , myList **value)
{
///make a outder clsas saying for every this prime number remove it and its multiples

    myList *current  = *value;
    int index = 0;
    int length = getSizeOfList(value);
    if(current == NULL)
    {
        printf("No items Present\n");
        return;
    }
    while(current != NULL)
    {
        if(current->val % deletingItem == 0 )
        {
        //ONLY  ITEM
        if(current->next == NULL && length == 1) //length == 1
        {
            free(*value);
            *value = NULL;
            return;
           // free(current);

        }
        else if ( index == 0) //fissrt item to remove
        {

            removeFirst(value);
        }
        else if(index == length){ //last item to remoce

            removeLast(value);
            return;
        }
        else
        {
            removeMiddle(value , current->val);
        }

    }
    index++;
    current = current->next;
}
return;

}

int getSizeOfList(myList **head)
{

    int count = 0;
    myList *tempNode = *head;

    while(tempNode != NULL)
    {
        count++;
        tempNode = tempNode->next;
    }
    return count;
}

void removeFirst(myList **value)
{
    if(getSizeOfList(value) == 1)
    {
        removeLast(value);
    }
    else
    {
        myList *next = NULL;
        next = (*value)->next;
        free(*value);
        *value = next;
        return;
    }

}

void removeLast(myList **value)
{
    myList *current = *value;
    if(current->next == NULL)
    {
        myList *next = NULL;
        next = (*value)->next;
        free(*value);
        *value = next;
        return;
    }
    while(current->next->next != NULL)
    {
        current = current->next;
    }
    current->next = NULL;
    free(current->next);
    return;
}


void removeMiddle (myList **value ,int item)
{
    myList *current = *value;
    //printf("THE value is %i\n" , current->val);
    if(current->val == item && current->next == NULL )
    {
        free(current);
        return;
    }
    while(current->next != NULL)
    {
        if(current->next->val == item)
        {
            myList *tempNode = current->next;
            current->next = tempNode->next;
            //tempNode->next = NULL;
            free(tempNode);
            return ;
        }
       // printf("ON VALEU %i\n" , current->val);
        current=current->next;
    }

}
