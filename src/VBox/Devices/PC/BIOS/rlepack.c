/* pack bmp file v0.1 */

#include <stdio.h>
#include <stdlib.h>


int main(int argc, char *argv[])
{
    FILE *in, *out;
    int c, n, i, size;
    int byte;


    if (argc < 2)
    {
        in = stdin;
        out = stdout;
    }
    else
    if (argc < 3)
    {
        in = fopen(argv[1], "rb");
        out = stdout;
    }
    else
    if (argc < 4)
    {
        in = fopen(argv[1], "rb");
        out = fopen(argv[2], "wb");
    }

    if ((byte = fgetc(in)) == EOF)
        return 0;

    fprintf(out, "PKXX");

    for(;;)
    {
        int count = 0;
        int next;

        if ((next = fgetc(in)) == EOF)
            break;

        if ((byte == next) ||
            (byte & 0xC0) == 0xC0)
        {
            // find other bytes
            count = 1;

            for(;;)
            {
                if (byte != next)
                    break;
                else
                    count++;

                if ((next = fgetc(in)) == EOF)
                    break;

                if (count >= 0x3F)
                    break;
            }
        }

        if (count == 0 && (byte & 0xC0) != 0xC0)
        {
            fputc(byte, out);
        }
        else
        {
            fputc((0xC0 | count), out);
            fputc(byte, out);
        }
        byte = next;
    }

    // write header
    fseek(out, 0, SEEK_END);
    size = ftell(out);
    fseek(out, 2, SEEK_SET);

    fwrite(&size, 2, 1, out);

    fclose(in);
    fclose(out);

    return 0;
}
