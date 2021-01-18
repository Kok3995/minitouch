#include <stdio.h>

int getChannel(int format) {
    switch (format)
    {
        case 1: 
        case 2: 
        case 5: 
            return 4;
        case 3:
            return 3;
        case 4:
            return 2;
        default: return 0;
    }
}

int ss(FILE* file) {
    FILE* o;
    size_t read_size;
    int w,h,f;
    int err = -1;
    size_t buff_size = 4096;
    o = popen("screencap", "r");

    if (o != NULL) {
        read_size = fread(&w, sizeof(int), 1, o);
        if (read_size > 0)
            fwrite(&w, sizeof(int), 1, file);

        read_size = fread(&h, sizeof(int), 1, o);
        if (read_size > 0)
            fwrite(&h, sizeof(int), 1, file);

        read_size = fread(&f, sizeof(int), 1, o);
        if (read_size > 0)
            fwrite(&f, sizeof(int), 1, file);

        int channel = getChannel(f);

        //FILE* raw = fopen("/data/local/tmp/a.raw", "w");

        if (w > 0 && h > 0 && f > 0) 
        {
            char buff[buff_size];

            while ((read_size = fread(&buff, 1, buff_size, o)) > 0) {
                fwrite(&buff, read_size, 1, file);
                fflush(file);
            }

            //fclose(raw);
        }
        
        pclose(o);
        return 1;
    }

    return 0;    
}