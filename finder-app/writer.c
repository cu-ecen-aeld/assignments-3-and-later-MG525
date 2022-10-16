#include "stdio.h"
#include "syslog.h"

int main(int argc, char *argv[])
{
    openlog(NULL, 0, LOG_USER);
    if (argc < 3)
    {
        syslog(LOG_ERR, "Incorrect Number of arguments: %d", argc);
        return 1;
    }
    else
    {
        const char *filename = argv[1];
        FILE *file = fopen(filename, "w+");
        if (file == NULL)
        {
            syslog(LOG_ERR, "Cannot open file: %s", argv[1]);
            return 1;
        }
        else
        {
            syslog(LOG_DEBUG, "Writing %s to %s", argv[2], argv[1]);
            fputs(argv[2], file);
            fclose(file);
        }
    }
    return 0;
}