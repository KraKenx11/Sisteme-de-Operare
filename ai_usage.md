I used Gemini for this project. For the first prompt, I told the AI: "You have the following structure: typedef struct Report{
    int report_id;
    char name[15];
    float latitude;
    float longitude;
    char issue_category[10];
    int severity;
    time_t timestamp;
    char description_text[50];
}Report;

You need to generate a function called int parse_condition(const char *input, char *field, char *op, char *value); which splits a field:operator:value string into its three parts." The AI generated the function using strtok for parsing and a series of if-else blocks for matching. I added a local temporary buffer because using strtok with argv directly would be bad, and used strcpy to copy the input. For safety, I implemented explicit checks for each token in case any of the values are missing.

For the second function, the prompt I used was: "Now, generate a function called int match_condition(Report *r, const char *field, const char *op, const char *value); which returns 1 if the record satisfies the condition and 0 otherwise." The AI generated a series of if-else statements here as well but I had to add a few things: One of the most important manual fixes was implementing atoi() for the severity and timestamp fields. The value received from the command line is a string, but the struct stores these as integers or time_t. I ensured the code converts the string to the correct C type before performing the comparison. Also, I manually verified that all six required operators (==, !=, <, <=, >, >=) were implemented for numerical fields.
