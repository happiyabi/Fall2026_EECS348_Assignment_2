/* =====================================================================
   email_priority.c
   -----------------------------------------------------------------
   A priority queue for a busy CEO's inbox, implemented from scratch
   using an array (list) based MaxHeap. No pre-built heap library is
   used anywhere in this file.

   Priority rules (highest to lowest):
       1. Boss
       2. Subordinate
       3. Peer
       4. ImportantPerson
       5. OtherPerson
   Within the same category, the NEWEST email (by date) is read first.

   Commands read from stdin (one per line):
       EMAIL <category>,<subject>,<date>   -> insert into heap
       NEXT                                 -> show highest priority email
                                                (does NOT remove it)
       READ                                  -> remove highest priority
                                                email (no output)
       COUNT                                 -> print number of unread
                                                emails
   ===================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ---------------------------------------------------------------------
   Email record
   --------------------------------------------------------------------- */
#define MAX_CATEGORY_LEN 32
#define MAX_SUBJECT_LEN 256
#define MAX_DATE_LEN 16
#define MAX_LINE_LEN 512

typedef struct
{
    char category[MAX_CATEGORY_LEN]; /* original category string      */
    char subject[MAX_SUBJECT_LEN];
    char date[MAX_DATE_LEN]; /* MM-DD-YYYY as given            */
    int categoryRank;        /* higher = more important        */
    long dateValue;          /* YYYYMMDD, higher = newer       */
} Email;

/* ---------------------------------------------------------------------
   MaxHeap (array/list based implementation)
   --------------------------------------------------------------------- */
typedef struct
{
    Email *data;  /* dynamic array of emails                        */
    int size;     /* current number of emails in the heap           */
    int capacity; /* current allocated capacity of the array        */
} MaxHeap;

/* ------------------------- Heap helper prototypes -------------------- */
static void heapInit(MaxHeap *h);
static void heapFree(MaxHeap *h);
static void heapEnsureCapacity(MaxHeap *h);
static void heapSwap(Email *a, Email *b);
static int emailIsHigherPriority(const Email *a, const Email *b);
static void heapifyUp(MaxHeap *h, int index);
static void heapifyDown(MaxHeap *h, int index);
static void heapInsert(MaxHeap *h, Email e);
static int heapExtractMax(MaxHeap *h, Email *out); /* returns 1 on success, 0 if empty */
static int heapPeekMax(const MaxHeap *h, Email *out);

/* ------------------------- Parsing helper prototypes ------------------ */
static int categoryRank(const char *category);
static long dateToValue(const char *date);
static void trimNewline(char *s);
static void printEmail(const Email *e);

/* =======================================================================
   MAIN
   ======================================================================= */
int main(void)
{
    MaxHeap heap;
    heapInit(&heap);

    char line[MAX_LINE_LEN];

    while (fgets(line, sizeof(line), stdin) != NULL)
    {
        trimNewline(line);

        /* Skip completely blank lines */
        if (line[0] == '\0')
        {
            continue;
        }

        if (strncmp(line, "EMAIL ", 6) == 0)
        {
            /* Rest of line after "EMAIL " : category,subject,date */
            char *rest = line + 6;

            char category[MAX_CATEGORY_LEN] = {0};
            char subject[MAX_SUBJECT_LEN] = {0};
            char date[MAX_DATE_LEN] = {0};

            /* First comma separates category */
            char *firstComma = strchr(rest, ',');
            if (firstComma == NULL)
            {
                /* Malformed line; skip it */
                continue;
            }
            /* Last comma separates date (subject itself has no commas,
               so the LAST comma in the string is the one before date) */
            char *lastComma = strrchr(rest, ',');
            if (lastComma == firstComma)
            {
                /* Only one comma found -> malformed (need category,subject,date) */
                continue;
            }

            size_t catLen = (size_t)(firstComma - rest);
            if (catLen >= sizeof(category))
                catLen = sizeof(category) - 1;
            strncpy(category, rest, catLen);
            category[catLen] = '\0';

            size_t subjLen = (size_t)(lastComma - (firstComma + 1));
            if (subjLen >= sizeof(subject))
                subjLen = sizeof(subject) - 1;
            strncpy(subject, firstComma + 1, subjLen);
            subject[subjLen] = '\0';

            strncpy(date, lastComma + 1, sizeof(date) - 1);
            date[sizeof(date) - 1] = '\0';

            Email e;
            strncpy(e.category, category, sizeof(e.category) - 1);
            e.category[sizeof(e.category) - 1] = '\0';
            strncpy(e.subject, subject, sizeof(e.subject) - 1);
            e.subject[sizeof(e.subject) - 1] = '\0';
            strncpy(e.date, date, sizeof(e.date) - 1);
            e.date[sizeof(e.date) - 1] = '\0';

            e.categoryRank = categoryRank(e.category);
            e.dateValue = dateToValue(e.date);

            heapInsert(&heap, e);
        }
        else if (strcmp(line, "NEXT") == 0)
        {
            Email e;
            if (heapPeekMax(&heap, &e))
            {
                printf("Next email:\n");
                printEmail(&e);
            }
            else
            {
                printf("No emails to read.\n");
            }
        }
        else if (strcmp(line, "READ") == 0)
        {
            Email e;
            /* Simply remove the top email; no output required.
               If the queue is empty, do nothing (no crash). */
            heapExtractMax(&heap, &e);
        }
        else if (strcmp(line, "COUNT") == 0)
        {
            printf("There are %d emails to read.\n", heap.size);
        }
        /* Any other/unknown line is silently ignored */
    }

    heapFree(&heap);
    return 0;
}

/* =======================================================================
   MaxHeap implementation (array / list based, built from scratch)
   ======================================================================= */

static void heapInit(MaxHeap *h)
{
    h->capacity = 8;
    h->size = 0;
    h->data = (Email *)malloc(sizeof(Email) * (size_t)h->capacity);
    if (h->data == NULL)
    {
        fprintf(stderr, "Memory allocation failed.\n");
        exit(1);
    }
}

static void heapFree(MaxHeap *h)
{
    free(h->data);
    h->data = NULL;
    h->size = 0;
    h->capacity = 0;
}

static void heapEnsureCapacity(MaxHeap *h)
{
    if (h->size >= h->capacity)
    {
        h->capacity *= 2;
        Email *newData = (Email *)realloc(h->data, sizeof(Email) * (size_t)h->capacity);
        if (newData == NULL)
        {
            fprintf(stderr, "Memory allocation failed.\n");
            exit(1);
        }
        h->data = newData;
    }
}

static void heapSwap(Email *a, Email *b)
{
    Email temp = *a;
    *a = *b;
    *b = temp;
}

/* Returns 1 if email 'a' should be considered higher priority (i.e.
   should sit closer to the root / be read sooner) than email 'b'.
   Rules:
     1. Higher categoryRank wins.
     2. If categories tie, the newer date (larger dateValue) wins. */
static int emailIsHigherPriority(const Email *a, const Email *b)
{
    if (a->categoryRank != b->categoryRank)
    {
        return a->categoryRank > b->categoryRank;
    }
    return a->dateValue > b->dateValue;
}

/* Bubble the element at 'index' up until heap property holds */
static void heapifyUp(MaxHeap *h, int index)
{
    while (index > 0)
    {
        int parent = (index - 1) / 2;
        if (emailIsHigherPriority(&h->data[index], &h->data[parent]))
        {
            heapSwap(&h->data[index], &h->data[parent]);
            index = parent;
        }
        else
        {
            break;
        }
    }
}

/* Push the element at 'index' down until heap property holds */
static void heapifyDown(MaxHeap *h, int index)
{
    while (1)
    {
        int left = 2 * index + 1;
        int right = 2 * index + 2;
        int largest = index;

        if (left < h->size && emailIsHigherPriority(&h->data[left], &h->data[largest]))
        {
            largest = left;
        }
        if (right < h->size && emailIsHigherPriority(&h->data[right], &h->data[largest]))
        {
            largest = right;
        }
        if (largest == index)
        {
            break;
        }
        heapSwap(&h->data[index], &h->data[largest]);
        index = largest;
    }
}

static void heapInsert(MaxHeap *h, Email e)
{
    heapEnsureCapacity(h);
    h->data[h->size] = e;
    heapifyUp(h, h->size);
    h->size++;
}

/* Removes and returns the highest priority email.
   Returns 1 on success, 0 if the heap was empty. */
static int heapExtractMax(MaxHeap *h, Email *out)
{
    if (h->size == 0)
    {
        return 0;
    }
    if (out != NULL)
    {
        *out = h->data[0];
    }
    h->size--;
    h->data[0] = h->data[h->size];
    heapifyDown(h, 0);
    return 1;
}

/* Returns the highest priority email WITHOUT removing it.
   Returns 1 on success, 0 if the heap is empty. */
static int heapPeekMax(const MaxHeap *h, Email *out)
{
    if (h->size == 0)
    {
        return 0;
    }
    if (out != NULL)
    {
        *out = h->data[0];
    }
    return 1;
}

/* =======================================================================
   Parsing / utility helpers
   ======================================================================= */

/* Maps a sender category string to a numeric rank.
   Higher number = higher priority (read sooner). */
static int categoryRank(const char *category)
{
    if (strcmp(category, "Boss") == 0)
        return 5;
    if (strcmp(category, "Subordinate") == 0)
        return 4;
    if (strcmp(category, "Peer") == 0)
        return 3;
    if (strcmp(category, "ImportantPerson") == 0)
        return 2;
    if (strcmp(category, "OtherPerson") == 0)
        return 1;
    return 0; /* unknown category - lowest priority, shouldn't happen */
}

/* Converts "MM-DD-YYYY" into an integer YYYYMMDD so that later dates
   compare as larger numbers. */
static long dateToValue(const char *date)
{
    int month = 0, day = 0, year = 0;
    if (sscanf(date, "%d-%d-%d", &month, &day, &year) != 3)
    {
        return 0;
    }
    return (long)year * 10000L + (long)month * 100L + (long)day;
}

/* Strips trailing \n and \r characters from a line read via fgets */
static void trimNewline(char *s)
{
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r'))
    {
        s[len - 1] = '\0';
        len--;
    }
}

static void printEmail(const Email *e)
{
    printf("Sender: %s\n", e->category);
    printf("Subject: %s\n", e->subject);
    printf("Date: %s\n", e->date);
}