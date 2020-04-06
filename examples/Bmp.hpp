#ifndef __BMP_H__
#define __BMP_H__

#include <cstdio>
#include <cstdlib>

using namespace std;

typedef unsigned char BMP_byte;

struct Color {
    BMP_byte blue;
    BMP_byte green;
    BMP_byte red;
};

/// @todo destructor !!!

struct Bmp {
private:
    int pos(int x, int y) {
        if (x<0) x=0;
        if (x>=width) x=width-1;
        if (y<0) y=0;
        if (y>=height) y=height-1;
        return ((height-y-1)*width +x);
    }
    
public:
    Color * data;
    int width;
    int height;
    
    Color & get(int x, int y) {
        return data[pos(x, y)];
    }
    
    void copy(const Bmp & src) {
        width = src.width;
        height = src.height;
        int size = 3 * (src.width * src.height);
        data = (Color *) (new BMP_byte[size]);
        for (int i=0; i<size; ++i) {
            *(((BMP_byte *) data)+i) = *(((BMP_byte *) src.data)+i);
        }
    }
    
    BMP_byte & B(int x, int y) {
        return *((BMP_byte *) &(data[pos(x, y)]));
    }
    BMP_byte & G(int x, int y) {
        // get address from 3byte Color, cast to "bytes" address, add +1
        // to Address (= 1byte step size) and get value of it
        return *( ((BMP_byte *) &(data[pos(x, y)]))+1 );
    }
    BMP_byte & R(int x, int y) {
        return *( ((BMP_byte *) &(data[pos(x, y)]))+2 );
    }
    
    void init(int pwidth, int pheight) {
        width  = pwidth;
        height = pheight;
        data = new Color[width*height];
    }
    
    void read(const char * filename) {
        FILE* f = fopen(filename, "rb");
        BMP_byte info[54];
        BMP_byte * row;
        int row_padded;
        
        fread(info, sizeof(BMP_byte), 54, f);
        
        width     = *( (int*) &info[18]);
        height    = *( (int*) &info[22]);
        int psize = *( (int*) &info[34]);
        
        data = new Color[width*height];
        
        row_padded = (psize/height);
        row = new BMP_byte[row_padded];
        
        /** sometimes the fileheader is bigger?!?! */
        //fread(row, sizeof(byte), 68, f);
        
        for(int y = height-1; y >= 0; --y) {
            fread(row, sizeof(BMP_byte), row_padded, f);
            for(int x = 0; x < width; ++x) {
                B(x, y) = row[(3*x)];
                G(x, y) = row[(3*x)+1];
                R(x, y) = row[(3*x)+2];
            }
        }
        fclose(f);
    }
    
    void write(const char * filename) {
        FILE* f = fopen(filename, "wb");
        BMP_byte info[54] = {
            'B','M',  0,0,0,0, 0,0, 0,0, 54,0,0,0,
            40,0,0,0, 0,0,0,0, 0,0,0,0,  1,0, 24,0,
            0,0,0,0,  0,0,0,0
        };
        int xbytes = 4 - ((width * 3) % 4);
        if (xbytes == 4) xbytes = 0;
        int psize = ((width * 3) + xbytes) * height;
        
        *( (int*) &info[2]) = 54 + psize;
        *( (int*) &info[18]) = width;
        *( (int*) &info[22]) = height;
        *( (int*) &info[34]) = psize;
        
        fwrite(info, sizeof(BMP_byte), sizeof(info), f);
        
        int x,y,n;
        for (y=height-1; y>=0; --y) {
            for (x=0; x<width; ++x) {
                fprintf(f, "%c", B(x, y));
                fprintf(f, "%c", G(x, y));
                fprintf(f, "%c", R(x, y));
            }
            // BMP lines must be of lengths divisible by 4
            if (xbytes) {
                for (n = 0; n < xbytes; ++n) fprintf(f, "%c", 0);
            }
        }
        
        fclose(f);
    }
};

#endif
