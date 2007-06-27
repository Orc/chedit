#ifndef PSF_MAGIC
#define PSF_MAGIC	0x436
#define PSF_MODE	0

typedef struct psf_header {
    int magic: 16;
    int mode :  8;
    int size :  8;
} PSF_header;

#endif PSF_MAGIC
