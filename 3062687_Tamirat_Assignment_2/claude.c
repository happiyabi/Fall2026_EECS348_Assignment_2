/*
Program: EECS 348 Assignment 2
Description: Prioritizes a CEO's emails using a from scratch, array-based
             MaxHeap. Orders by sender category (Boss > Subordinate > Peer
             > ImportantPerson > OtherPerson), then by newest date
Inputs: Stdin commands, EMAIL <category>,<subject>,<date>, NEXT, READ, COUNT
Outputs: Terminal output for NEXT/COUNT; stderr warnings for bad input
Collaborators: None
Other Sources: None
Author: Yabsira Tamirat
Creation Date: September 17, 2026
Revised: September 17, 2026
Revisions: Bounds-checked copies (strncpy) to prevent overflows; cache
           category rank/date value at insertion for O(1) comparisons;
           stderr warnings for malformed input; named constant for
           initial capacity; renamed helpers to avoid clashing with
           struct field names; insert by const pointer to avoid a copy
*/

#include <stdio.h>  // Includes functions for terminal input/output (printf, fgets, sscanf, fprintf)
#include <stdlib.h> // Includes memory functions (malloc, realloc, free) and exit()
#include <string.h> // Includes string functions (strcmp, strncmp, strncpy, strchr, strrchr, strlen)
#include <ctype.h>  // Included for character-handling utilities (kept for portability with input parsing)

/* ---------------------------------------------------------------------
   Email record
   --------------------------------------------------------------------- */
#define MAX_CATEGORY_LEN 32     // Sets the maximum length allowed for a sender category string
#define MAX_SUBJECT_LEN 256     // Sets the maximum length allowed for a subject line
#define MAX_DATE_LEN 16         // Sets the maximum length allowed for a date string (MM-DD-YYYY)
#define MAX_LINE_LEN 512        // Sets the maximum length allowed for one full line of input
#define INITIAL_HEAP_CAPACITY 8 // Sets the starting number of heap slots before any growth is needed

typedef struct
{                                    // Defines the structure that stores one email's data
    char category[MAX_CATEGORY_LEN]; // Stores the original sender category string (e.g. "Boss")
    char subject[MAX_SUBJECT_LEN];   // Stores the subject line text of the email
    char date[MAX_DATE_LEN];         // Stores the date string exactly as given (MM-DD-YYYY)
    int categoryRank;                // Stores the precomputed numeric priority rank (higher = more important)
    long dateValue;                  // Stores the precomputed date as YYYYMMDD (higher = newer)
} Email;                             // Names this structure type "Email" for use throughout the program

/* ---------------------------------------------------------------------
   MaxHeap (array/list based implementation)
   --------------------------------------------------------------------- */
typedef struct
{                 // Defines the structure that stores the MaxHeap itself
    Email *data;  // Points to the dynamically allocated array of emails backing the heap
    int size;     // Stores the current number of emails stored in the heap
    int capacity; // Stores the current allocated capacity of the data array
} MaxHeap;        // Names this structure type "MaxHeap" for use throughout the program

/* ------------------------- Heap helper prototypes -------------------- */
static void heapInit(MaxHeap *h);                                 // Declares the function that initializes a new empty heap
static void heapFree(MaxHeap *h);                                 // Declares the function that releases a heap's memory
static void heapEnsureCapacity(MaxHeap *h);                       // Declares the function that grows the heap array when full
static void heapSwap(Email *a, Email *b);                         // Declares the function that swaps two Email structs in place
static int emailIsHigherPriority(const Email *a, const Email *b); // Declares the function comparing two emails' priority
static void heapifyUp(MaxHeap *h, int index);                     // Declares the function that restores heap order upward
static void heapifyDown(MaxHeap *h, int index);                   // Declares the function that restores heap order downward
static void heapInsert(MaxHeap *h, const Email *e);               // Declares the function that adds a new email into the heap
static int heapExtractMax(MaxHeap *h, Email *out);                // Declares the function that removes and returns the top email
static int heapPeekMax(const MaxHeap *h, Email *out);             // Declares the function that reads the top email without removing it

/* ------------------------- Parsing helper prototypes ------------------ */
static int getCategoryRank(const char *category); // Declares the function that maps a category string to a priority number
static long getDateValue(const char *date);       // Declares the function that converts a date string into a comparable integer
static void trimNewline(char *s);                 // Declares the function that strips trailing newline characters from a string
static void printEmail(const Email *e);           // Declares the function that prints one email's fields to the terminal

/* =======================================================================
   MAIN
   ======================================================================= */
int main(void) // Starts the main function of the program
{
    MaxHeap heap;    // Declares the single MaxHeap instance that stores the CEO's unread emails
    heapInit(&heap); // Initializes the heap with its starting capacity and an empty size

    char line[MAX_LINE_LEN]; // Declares a buffer to hold each line of input as it is read

    // Repeats reading and processing commands until the end of input is reached
    while (fgets(line, sizeof(line), stdin) != NULL) // Reads one line at a time from standard input into 'line'
    {
        trimNewline(line); // Removes any trailing \n or \r characters from the line just read

        // Checks whether the line read was completely empty after trimming
        if (line[0] == '\0') // Tests if the first character of the line is the null terminator
        {
            continue; // Skips this blank line and reads the next one
        }

        // Checks whether the line is an EMAIL command
        if (strncmp(line, "EMAIL ", 6) == 0) // Compares the first 6 characters of the line to "EMAIL "
        {
            char *rest = line + 6; // Points to the remainder of the line after the "EMAIL " prefix

            char category[MAX_CATEGORY_LEN] = {0}; // Declares and zero-initializes a buffer for the parsed category
            char subject[MAX_SUBJECT_LEN] = {0};   // Declares and zero-initializes a buffer for the parsed subject
            char date[MAX_DATE_LEN] = {0};         // Declares and zero-initializes a buffer for the parsed date

            char *firstComma = strchr(rest, ','); // Finds the first comma, which separates the category field
            // Checks whether a first comma was found at all
            if (firstComma == NULL) // Tests if strchr failed to find any comma in the line
            {
                fprintf(stderr, "Warning: malformed EMAIL line (no comma found), skipped: %s\n", rest); // Warns on stderr about the malformed line
                continue;                                                                               // Skips this line and moves on to the next one
            }

            char *lastComma = strrchr(rest, ','); // Finds the last comma, which separates the date field
            // Checks whether only one comma exists (meaning the subject or date field is missing)
            if (lastComma == firstComma) // Tests if the first and last comma found are the same comma
            {
                fprintf(stderr, "Warning: malformed EMAIL line (need category,subject,date), skipped: %s\n", rest); // Warns on stderr about the malformed line
                continue;                                                                                           // Skips this line and moves on to the next one
            }

            size_t catLen = (size_t)(firstComma - rest); // Computes the length of the category text before the first comma
            // Checks whether the category text is too long for its buffer
            if (catLen >= sizeof(category))
                catLen = sizeof(category) - 1; // Caps the length so it fits with room for the null terminator
            strncpy(category, rest, catLen);   // Copies only catLen characters of the category text into the buffer
            category[catLen] = '\0';           // Manually null-terminates the category string since strncpy may not

            size_t subjLen = (size_t)(lastComma - (firstComma + 1)); // Computes the length of the subject text between the two commas
            // Checks whether the subject text is too long for its buffer
            if (subjLen >= sizeof(subject))
                subjLen = sizeof(subject) - 1;         // Caps the length so it fits with room for the null terminator
            strncpy(subject, firstComma + 1, subjLen); // Copies only subjLen characters of the subject text into the buffer
            subject[subjLen] = '\0';                   // Manually null-terminates the subject string since strncpy may not

            strncpy(date, lastComma + 1, sizeof(date) - 1); // Copies the date text after the last comma into the buffer, capped to its size
            date[sizeof(date) - 1] = '\0';                  // Manually null-terminates the date string to guarantee it always ends properly

            Email e;                                               // Declares a new Email struct to hold this parsed email's data
            strncpy(e.category, category, sizeof(e.category) - 1); // Copies the parsed category into the Email struct safely
            e.category[sizeof(e.category) - 1] = '\0';             // Ensures the category field is always null-terminated
            strncpy(e.subject, subject, sizeof(e.subject) - 1);    // Copies the parsed subject into the Email struct safely
            e.subject[sizeof(e.subject) - 1] = '\0';               // Ensures the subject field is always null-terminated
            strncpy(e.date, date, sizeof(e.date) - 1);             // Copies the parsed date into the Email struct safely
            e.date[sizeof(e.date) - 1] = '\0';                     // Ensures the date field is always null-terminated

            e.categoryRank = getCategoryRank(e.category); // Precomputes and stores this email's numeric priority rank
            // Checks whether the category string was not one of the five recognized categories
            if (e.categoryRank == 0) // Tests if getCategoryRank returned the "unknown" fallback value
            {
                fprintf(stderr, "Warning: unrecognized sender category \"%s\"; treated as lowest priority.\n", e.category); // Warns on stderr about the unrecognized category
            }

            e.dateValue = getDateValue(e.date); // Precomputes and stores this email's date as a comparable integer
            // Checks whether the date string could not be parsed into three numeric parts
            if (e.dateValue == 0) // Tests if getDateValue returned the "unparsable" fallback value
            {
                fprintf(stderr, "Warning: unparsable date \"%s\"; treated as oldest.\n", e.date); // Warns on stderr about the unparsable date
            }

            heapInsert(&heap, &e); // Inserts the newly built email into the MaxHeap
        }
        // Checks whether the line is a NEXT command
        else if (strcmp(line, "NEXT") == 0) // Compares the trimmed line exactly to "NEXT"
        {
            Email e; // Declares a temporary Email struct to receive the top email's data
            // Checks whether the heap has at least one email to show
            if (heapPeekMax(&heap, &e)) // Calls heapPeekMax and tests whether it found an email
            {
                printf("Next email:\n"); // Prints the header line announcing the next email
                printEmail(&e);          // Prints the Sender, Subject, and Date fields of the top email
            }
            else // Handles the case where the heap is empty
            {
                printf("No emails to read.\n"); // Informs the user that there are no emails left to show
            }
        }
        // Checks whether the line is a READ command
        else if (strcmp(line, "READ") == 0) // Compares the trimmed line exactly to "READ"
        {
            Email e;                   // Declares a temporary Email struct to receive the removed email's data (unused for output)
            heapExtractMax(&heap, &e); // Removes the top-priority email from the heap; does nothing if the heap is empty
        }
        // Checks whether the line is a COUNT command
        else if (strcmp(line, "COUNT") == 0) // Compares the trimmed line exactly to "COUNT"
        {
            printf("There are %d emails to read.\n", heap.size); // Prints the current number of unread emails in the heap
        }
        // Any other/unknown line falls through here and is silently ignored
    }

    heapFree(&heap); // Releases the heap's dynamically allocated memory before the program ends
    return 0;        // Ends the program and returns success status to the operating system
}

/* =======================================================================
   MaxHeap implementation (array / list based, built from scratch)
   ======================================================================= */

static void heapInit(MaxHeap *h) // Defines the function that sets up a brand-new empty heap
{
    h->capacity = INITIAL_HEAP_CAPACITY;                            // Sets the heap's starting capacity to the named constant
    h->size = 0;                                                    // Sets the heap's initial size to zero emails
    h->data = (Email *)malloc(sizeof(Email) * (size_t)h->capacity); // Allocates the initial array of Email slots
    // Checks whether the memory allocation failed
    if (h->data == NULL) // Tests if malloc returned a null pointer
    {
        fprintf(stderr, "Memory allocation failed.\n"); // Reports the allocation failure to stderr
        exit(1);                                        // Terminates the program immediately since it cannot continue without memory
    }
}

static void heapFree(MaxHeap *h) // Defines the function that cleans up a heap's memory
{
    free(h->data);   // Frees the dynamically allocated array of emails
    h->data = NULL;  // Sets the pointer to NULL to avoid accidental use after freeing
    h->size = 0;     // Resets the size to zero since the heap is now empty
    h->capacity = 0; // Resets the capacity to zero since no memory is allocated
}

// Amortized O(1): doubling means the total cost of all resizes across
// n inserts stays O(n), even though any single resize is O(n).
static void heapEnsureCapacity(MaxHeap *h) // Defines the function that grows the heap array when it becomes full
{
    // Checks whether the heap has reached its current capacity
    if (h->size >= h->capacity) // Tests if there is no more room to add another email
    {
        h->capacity *= 2;                                                                // Doubles the capacity to make room for future growth
        Email *newData = (Email *)realloc(h->data, sizeof(Email) * (size_t)h->capacity); // Reallocates the array to the new, larger capacity
        // Checks whether the reallocation failed
        if (newData == NULL) // Tests if realloc returned a null pointer
        {
            fprintf(stderr, "Memory allocation failed.\n"); // Reports the allocation failure to stderr
            exit(1);                                        // Terminates the program immediately since it cannot continue without memory
        }
        h->data = newData; // Updates the heap's data pointer to the newly (re)allocated array
    }
}

static void heapSwap(Email *a, Email *b) // Defines the function that exchanges two Email structs in place
{
    Email temp = *a; // Copies the contents pointed to by 'a' into a temporary Email
    *a = *b;         // Copies the contents pointed to by 'b' into the location pointed to by 'a'
    *b = temp;       // Copies the saved temporary contents into the location pointed to by 'b'
}

// Returns 1 if email 'a' should be considered higher priority (i.e.
// should sit closer to the root / be read sooner) than email 'b'.
// Rules:
//   1. Higher categoryRank wins.
//   2. If categories tie, the newer date (larger dateValue) wins.
// Runs in O(1) since both fields are precomputed integers.
static int emailIsHigherPriority(const Email *a, const Email *b) // Defines the function that compares two emails' priority
{
    // Checks whether the two emails belong to different priority categories
    if (a->categoryRank != b->categoryRank) // Tests if the categoryRank fields differ
    {
        return a->categoryRank > b->categoryRank; // Returns true if 'a' has the higher category rank
    }
    return a->dateValue > b->dateValue; // Since categories tie, returns true if 'a' has the newer (larger) date value
}

// Bubble the element at 'index' up until heap property holds.
// O(log n): at most tree-height swaps.
static void heapifyUp(MaxHeap *h, int index) // Defines the function that restores heap order by moving an element upward
{
    // Repeats as long as the current index has not reached the root of the heap
    while (index > 0) // Tests whether there is still a parent above the current index
    {
        int parent = (index - 1) / 2; // Computes the array index of the current element's parent
        // Checks whether the current element has higher priority than its parent
        if (emailIsHigherPriority(&h->data[index], &h->data[parent])) // Tests if a swap with the parent is needed
        {
            heapSwap(&h->data[index], &h->data[parent]); // Swaps the current element with its parent
            index = parent;                              // Moves the working index up to the parent's position and continues
        }
        else // Handles the case where the heap property already holds
        {
            break; // Stops the loop since no further swaps are needed
        }
    }
}

// Push the element at 'index' down until heap property holds.
// O(log n): at most tree-height swaps.
static void heapifyDown(MaxHeap *h, int index) // Defines the function that restores heap order by moving an element downward
{
    // Repeats indefinitely until an internal break statement stops it
    while (1) // Creates an infinite loop that is exited explicitly below
    {
        int left = 2 * index + 1;  // Computes the array index of the current element's left child
        int right = 2 * index + 2; // Computes the array index of the current element's right child
        int largest = index;       // Assumes, for now, that the current element itself is the largest

        // Checks whether a left child exists and has higher priority than the current largest
        if (left < h->size && emailIsHigherPriority(&h->data[left], &h->data[largest])) // Tests both bounds and priority for the left child
        {
            largest = left; // Updates 'largest' to point to the left child instead
        }
        // Checks whether a right child exists and has higher priority than the current largest
        if (right < h->size && emailIsHigherPriority(&h->data[right], &h->data[largest])) // Tests both bounds and priority for the right child
        {
            largest = right; // Updates 'largest' to point to the right child instead
        }
        // Checks whether the current element is already the largest of the three
        if (largest == index) // Tests if no child had higher priority than the current element
        {
            break; // Stops the loop since the heap property now holds at this position
        }
        heapSwap(&h->data[index], &h->data[largest]); // Swaps the current element with whichever child had higher priority
        index = largest;                              // Moves the working index down to the child's position and continues
    }
}

// O(log n) amortized (O(1) copy + O(log n) heapifyUp). Takes a const
// pointer rather than a full Email by value to avoid copying the
// ~300+ byte struct twice (once into the parameter, once into the
// array) on every single insertion.
static void heapInsert(MaxHeap *h, const Email *e) // Defines the function that adds a new email into the heap
{
    heapEnsureCapacity(h); // Makes sure there is room in the array before inserting
    h->data[h->size] = *e; // Copies the new email into the next free slot at the end of the array
    heapifyUp(h, h->size); // Restores the heap property by bubbling the new email upward as needed
    h->size++;             // Increases the recorded heap size now that the new email has been added
}

// Removes and returns the highest priority email. O(log n).
// Returns 1 on success, 0 if the heap was empty.
static int heapExtractMax(MaxHeap *h, Email *out) // Defines the function that removes the top-priority email from the heap
{
    // Checks whether the heap currently contains no emails
    if (h->size == 0) // Tests if the heap's size is zero
    {
        return 0; // Returns 0 to indicate there was nothing to extract
    }
    // Checks whether the caller wants a copy of the removed email
    if (out != NULL) // Tests if the caller passed a non-null output pointer
    {
        *out = h->data[0]; // Copies the root (highest priority) email into the caller's output variable
    }
    h->size--;                     // Decreases the heap size by one since an email is being removed
    h->data[0] = h->data[h->size]; // Moves the last element in the array into the now-empty root position
    heapifyDown(h, 0);             // Restores the heap property by sifting the new root downward as needed
    return 1;                      // Returns 1 to indicate the extraction succeeded
}

// Returns the highest priority email WITHOUT removing it. O(1).
// Returns 1 on success, 0 if the heap is empty.
static int heapPeekMax(const MaxHeap *h, Email *out) // Defines the function that reads the top email without removing it
{
    // Checks whether the heap currently contains no emails
    if (h->size == 0) // Tests if the heap's size is zero
    {
        return 0; // Returns 0 to indicate there is nothing to peek at
    }
    // Checks whether the caller wants a copy of the top email
    if (out != NULL) // Tests if the caller passed a non-null output pointer
    {
        *out = h->data[0]; // Copies the root (highest priority) email into the caller's output variable
    }
    return 1; // Returns 1 to indicate the peek succeeded
}

/* =======================================================================
   Parsing / utility helpers
   ======================================================================= */

// Maps a sender category string to a numeric rank.
// Higher number = higher priority (read sooner).
// Named getCategoryRank (rather than categoryRank) to avoid reading
// like the Email.categoryRank struct field it populates.
static int getCategoryRank(const char *category) // Defines the function that converts a category string into a priority number
{
    // Checks whether the category string matches "Boss"
    if (strcmp(category, "Boss") == 0)
        return 5; // Returns the highest rank for the Boss category
    // Checks whether the category string matches "Subordinate"
    if (strcmp(category, "Subordinate") == 0)
        return 4; // Returns the second-highest rank for the Subordinate category
    // Checks whether the category string matches "Peer"
    if (strcmp(category, "Peer") == 0)
        return 3; // Returns the middle rank for the Peer category
    // Checks whether the category string matches "ImportantPerson"
    if (strcmp(category, "ImportantPerson") == 0)
        return 2; // Returns the second-lowest rank for the ImportantPerson category
    // Checks whether the category string matches "OtherPerson"
    if (strcmp(category, "OtherPerson") == 0)
        return 1; // Returns the lowest defined rank for the OtherPerson category
    return 0;     // Returns 0 as a fallback if the category did not match any known string
}

// Converts "MM-DD-YYYY" into an integer YYYYMMDD so that later dates
// compare as larger numbers.
static long getDateValue(const char *date) // Defines the function that converts a date string into a comparable integer
{
    int month = 0, day = 0, year = 0; // Declares and zero-initializes variables to hold the parsed month, day, and year
    // Checks whether the date string could be parsed into exactly three numeric fields
    if (sscanf(date, "%d-%d-%d", &month, &day, &year) != 3) // Tests whether sscanf successfully filled all three variables
    {
        return 0; // Returns 0 as a fallback if the date could not be parsed correctly
    }
    return (long)year * 10000L + (long)month * 100L + (long)day; // Combines year, month, and day into a single YYYYMMDD integer
}

// Strips trailing \n and \r characters from a line read via fgets
static void trimNewline(char *s) // Defines the function that removes trailing newline characters from a string
{
    size_t len = strlen(s); // Computes the current length of the string
    // Repeats as long as the string is non-empty and ends in a newline or carriage return
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) // Tests the last character of the string each time through the loop
    {
        s[len - 1] = '\0'; // Overwrites the trailing newline/carriage-return character with a null terminator
        len--;             // Decreases the tracked length to reflect the character that was just removed
    }
}

static void printEmail(const Email *e) // Defines the function that prints one email's fields to the terminal
{
    printf("Sender: %s\n", e->category); // Prints the email's sender category as the "Sender" line
    printf("Subject: %s\n", e->subject); // Prints the email's subject text as the "Subject" line
    printf("Date: %s\n", e->date);       // Prints the email's date string as the "Date" line
}