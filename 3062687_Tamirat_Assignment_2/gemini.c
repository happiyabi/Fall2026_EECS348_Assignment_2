#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    char sender[32];
    char subject[256];
    char date[16]; // MM-DD-YYYY
} Email;

typedef struct
{
    Email *data;
    int capacity;
    int size;
} MaxHeap;

// Helper: Convert sender category to numerical rank (Higher = Greater Priority)
int get_sender_rank(const char *sender)
{
    if (strcmp(sender, "Boss") == 0)
        return 5;
    if (strcmp(sender, "Subordinate") == 0)
        return 4;
    if (strcmp(sender, "Peer") == 0)
        return 3;
    if (strcmp(sender, "ImportantPerson") == 0)
        return 2;
    if (strcmp(sender, "OtherPerson") == 0)
        return 1;
    return 0;
}

// Helper: Convert MM-DD-YYYY string to YYYYMMDD integer for direct comparison
int get_date_value(const char *date)
{
    int m, d, y;
    sscanf(date, "%d-%d-%d", &m, &d, &y);
    return y * 10000 + m * 100 + d;
}

// Returns >0 if e1 has higher priority than e2
int compare_emails(Email e1, Email e2)
{
    int rank1 = get_sender_rank(e1.sender);
    int rank2 = get_sender_rank(e2.sender);

    if (rank1 != rank2)
    {
        return rank1 - rank2;
    }

    // Tie-breaker: Newer date gets higher priority
    return get_date_value(e1.date) - get_date_value(e2.date);
}

// MaxHeap Initialization
MaxHeap *create_heap(int capacity)
{
    MaxHeap *heap = (MaxHeap *)malloc(sizeof(MaxHeap));
    heap->capacity = capacity;
    heap->size = 0;
    heap->data = (Email *)malloc(sizeof(Email) * capacity);
    return heap;
}

void swap(Email *a, Email *b)
{
    Email temp = *a;
    *a = *b;
    *b = temp;
}

void heapify_up(MaxHeap *heap, int index)
{
    while (index > 0)
    {
        int parent = (index - 1) / 2;
        if (compare_emails(heap->data[index], heap->data[parent]) > 0)
        {
            swap(&heap->data[index], &heap->data[parent]);
            index = parent;
        }
        else
        {
            break;
        }
    }
}

void heapify_down(MaxHeap *heap, int index)
{
    while (2 * index + 1 < heap->size)
    {
        int left = 2 * index + 1;
        int right = 2 * index + 2;
        int largest = index;

        if (left < heap->size && compare_emails(heap->data[left], heap->data[largest]) > 0)
        {
            largest = left;
        }
        if (right < heap->size && compare_emails(heap->data[right], heap->data[largest]) > 0)
        {
            largest = right;
        }

        if (largest != index)
        {
            swap(&heap->data[index], &heap->data[largest]);
            index = largest;
        }
        else
        {
            break;
        }
    }
}

void push(MaxHeap *heap, Email email)
{
    if (heap->size == heap->capacity)
    {
        heap->capacity *= 2;
        heap->data = (Email *)realloc(heap->data, sizeof(Email) * heap->capacity);
    }
    heap->data[heap->size] = email;
    heapify_up(heap, heap->size);
    heap->size++;
}

void pop(MaxHeap *heap)
{
    if (heap->size == 0)
        return;
    heap->data[0] = heap->data[heap->size - 1];
    heap->size--;
    heapify_down(heap, 0);
}

Email *peek(MaxHeap *heap)
{
    if (heap->size == 0)
        return NULL;
    return &heap->data[0];
}

void free_heap(MaxHeap *heap)
{
    free(heap->data);
    free(heap);
}

int main()
{
    MaxHeap *heap = create_heap(10);
    char line[512];

    while (fgets(line, sizeof(line), stdin))
    {
        // Strip trailing newline character
        line[strcspn(line, "\r\n")] = 0;

        if (strncmp(line, "EMAIL ", 6) == 0)
        {
            Email e;
            char *payload = line + 6;

            // Parse comma-delimited fields
            char *sender = strtok(payload, ",");
            char *subject = strtok(NULL, ",");
            char *date = strtok(NULL, ",");

            if (sender && subject && date)
            {
                // Trim leading whitespace from tokens if present
                while (*sender == ' ')
                    sender++;
                while (*subject == ' ')
                    subject++;
                while (*date == ' ')
                    date++;

                strcpy(e.sender, sender);
                strcpy(e.subject, subject);
                strcpy(e.date, date);
                push(heap, e);
            }
        }
        else if (strcmp(line, "COUNT") == 0)
        {
            printf("There are %d emails to read.\n", heap->size);
        }
        else if (strcmp(line, "NEXT") == 0)
        {
            Email *next_email = peek(heap);
            if (next_email)
            {
                printf("Next email:\nSender: %s\nSubject: %s\nDate: %s\n",
                       next_email->sender, next_email->subject, next_email->date);
            }
        }
        else if (strcmp(line, "READ") == 0)
        {
            pop(heap);
        }
    }

    free_heap(heap);
    return 0;
}