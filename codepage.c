/* CODEPAGE.C   (c) Copyright Jan Jaeger, 1999-2002                  */
/*              Code Page conversion                                 */

#include "hercules.h"


unsigned char
ascii_to_ebcdic[] = {
    "\x00\x01\x02\x03\x37\x2D\x2E\x2F\x16\x05\x25\x0B\x0C\x0D\x0E\x0F"
    "\x10\x11\x12\x13\x3C\x3D\x32\x26\x18\x19\x1A\x27\x22\x1D\x35\x1F"
    "\x40\x5A\x7F\x7B\x5B\x6C\x50\x7D\x4D\x5D\x5C\x4E\x6B\x60\x4B\x61"
    "\xF0\xF1\xF2\xF3\xF4\xF5\xF6\xF7\xF8\xF9\x7A\x5E\x4C\x7E\x6E\x6F"
    "\x7C\xC1\xC2\xC3\xC4\xC5\xC6\xC7\xC8\xC9\xD1\xD2\xD3\xD4\xD5\xD6"
    "\xD7\xD8\xD9\xE2\xE3\xE4\xE5\xE6\xE7\xE8\xE9\xAD\xE0\xBD\x5F\x6D"
    "\x79\x81\x82\x83\x84\x85\x86\x87\x88\x89\x91\x92\x93\x94\x95\x96"
    "\x97\x98\x99\xA2\xA3\xA4\xA5\xA6\xA7\xA8\xA9\xC0\x6A\xD0\xA1\x07"
    "\x68\xDC\x51\x42\x43\x44\x47\x48\x52\x53\x54\x57\x56\x58\x63\x67"
    "\x71\x9C\x9E\xCB\xCC\xCD\xDB\xDD\xDF\xEC\xFC\xB0\xB1\xB2\xB3\xB4"
    "\x45\x55\xCE\xDE\x49\x69\x04\x06\xAB\x08\xBA\xB8\xB7\xAA\x8A\x8B"
    "\x09\x0A\x14\xBB\x15\xB5\xB6\x17\x1B\xB9\x1C\x1E\xBC\x20\xBE\xBF"
    "\x21\x23\x24\x28\x29\x2A\x2B\x2C\x30\x31\xCA\x33\x34\x36\x38\xCF"
    "\x39\x3A\x3B\x3E\x41\x46\x4A\x4F\x59\x62\xDA\x64\x65\x66\x70\x72"
    "\x73\xE1\x74\x75\x76\x77\x78\x80\x8C\x8D\x8E\xEB\x8F\xED\xEE\xEF"
    "\x90\x9A\x9B\x9D\x9F\xA0\xAC\xAE\xAF\xFD\xFE\xFB\x3F\xEA\xFA\xFF"
};


unsigned char
ebcdic_to_ascii[] = {
    "\x00\x01\x02\x03\xA6\x09\xA7\x7F\xA9\xB0\xB1\x0B\x0C\x0D\x0E\x0F"
    "\x10\x11\x12\x13\xB2\x0A\x08\xB7\x18\x19\x1A\xB8\xBA\x1D\xBB\x1F"
    "\xBD\xC0\x1C\xC1\xC2\x0A\x17\x1B\xC3\xC4\xC5\xC6\xC7\x05\x06\x07"
    "\xC8\xC9\x16\xCB\xCC\x1E\xCD\x04\xCE\xD0\xD1\xD2\x14\x15\xD3\xFC"
    "\x20\xD4\x83\x84\x85\xA0\xD5\x86\x87\xA4\xD6\x2E\x3C\x28\x2B\xD7"
    "\x26\x82\x88\x89\x8A\xA1\x8C\x8B\x8D\xD8\x21\x24\x2A\x29\x3B\x5E"
    "\x2D\x2F\xD9\x8E\xDB\xDC\xDD\x8F\x80\xA5\x7C\x2C\x25\x5F\x3E\x3F"
    "\xDE\x90\xDF\xE0\xE2\xE3\xE4\xE5\xE6\x60\x3A\x23\x40\x27\x3D\x22"
    "\xE7\x61\x62\x63\x64\x65\x66\x67\x68\x69\xAE\xAF\xE8\xE9\xEA\xEC"
    "\xF0\x6A\x6B\x6C\x6D\x6E\x6F\x70\x71\x72\xF1\xF2\x91\xF3\x92\xF4"
    "\xF5\x7E\x73\x74\x75\x76\x77\x78\x79\x7A\xAD\xA8\xF6\x5B\xF7\xF8"
    "\x9B\x9C\x9D\x9E\x9F\xB5\xB6\xAC\xAB\xB9\xAA\xB3\xBC\x5D\xBE\xBF"
    "\x7B\x41\x42\x43\x44\x45\x46\x47\x48\x49\xCA\x93\x94\x95\xA2\xCF"
    "\x7D\x4A\x4B\x4C\x4D\x4E\x4F\x50\x51\x52\xDA\x96\x81\x97\xA3\x98"
    "\x5C\xE1\x53\x54\x55\x56\x57\x58\x59\x5A\xFD\xEB\x99\xED\xEE\xEF"
    "\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39\xFE\xFB\x9A\xF9\xFA\xFF"
};


unsigned char
cp_437_to_037[] = {
    "\x00\x01\x02\x03\x37\x2D\x2E\x2F\x16\x05\x15\x0B\x0C\x0D\x0E\x0F"
    "\x10\x11\x12\x13\x3C\x3D\x32\x26\x18\x19\x3F\x27\x22\x1D\x1E\x1F"
    "\x40\x5A\x7F\x7B\x5B\x6C\x50\x7D\x4D\x5D\x5C\x4E\x6B\x60\x4B\x61"
    "\xF0\xF1\xF2\xF3\xF4\xF5\xF6\xF7\xF8\xF9\x7A\x5E\x4C\x7E\x6E\x6F"
    "\x7C\xC1\xC2\xC3\xC4\xC5\xC6\xC7\xC8\xC9\xD1\xD2\xD3\xD4\xD5\xD6"
    "\xD7\xD8\xD9\xE2\xE3\xE4\xE5\xE6\xE7\xE8\xE9\xBA\xE0\xBB\xB0\x6D"
    "\x79\x81\x82\x83\x84\x85\x86\x87\x88\x89\x91\x92\x93\x94\x95\x96"
    "\x97\x98\x99\xA2\xA3\xA4\xA5\xA6\xA7\xA8\xA9\xC0\x4F\xD0\xA1\x07"
    "\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F"
    "\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F"
    "\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F"
    "\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F"
    "\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F"
    "\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F"
    "\x3F\x59\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F"
    "\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x90\x3F\x3F\x3F\x3F\xEA\x3F\xFF"
};


unsigned char
cp_037_to_437[] = {
    "\x00\x01\x02\x03\x07\x09\x07\x7F\x07\x07\x07\x0B\x0C\x0D\x0E\x0F"
    "\x10\x11\x12\x13\x07\x0A\x08\x07\x18\x19\x07\x07\x07\x07\x07\x07"
    "\x07\x07\x1C\x07\x07\x0A\x17\x1B\x07\x07\x07\x07\x07\x05\x06\x07"
    "\x07\x07\x16\x07\x07\x07\x07\x04\x07\x07\x07\x07\x14\x15\x07\x1A"
    "\x20\xFF\x83\x84\x85\xA0\x07\x86\x87\xA4\x9B\x2E\x3C\x28\x2B\x7C"
    "\x26\x82\x88\x89\x8A\xA1\x8C\x07\x8D\xE1\x21\x24\x2A\x29\x3B\xAA"
    "\x2D\x2F\x07\x8E\x07\x07\x07\x8F\x80\xA5\x07\x2C\x25\x5F\x3E\x3F"
    "\x07\x90\x07\x07\x07\x07\x07\x07\x70\x60\x3A\x23\x40\x27\x3D\x22"
    "\x07\x61\x62\x63\x64\x65\x66\x67\x68\x69\xAE\xAF\x07\x07\x07\xF1"
    "\xF8\x6A\x6B\x6C\x6D\x6E\x6F\x70\x71\x72\xA6\xA7\x91\x07\x92\x07"
    "\xE6\x7E\x73\x74\x75\x76\x77\x78\x79\x7A\xAD\xAB\x07\x07\x07\x07"
    "\x5E\x9C\x9D\xFA\x07\x07\x07\xAC\xAB\x07\x5B\x5D\x07\x07\x07\x07"
    "\x7B\x41\x42\x43\x44\x45\x46\x47\x48\x49\x07\x93\x94\x95\xA2\x07"
    "\x7D\x4A\x4B\x4C\x4D\x4E\x4F\x50\x51\x52\x07\x96\x81\x97\xA3\x98"
    "\x5C\xF6\x53\x54\x55\x56\x57\x58\x59\x5A\xFD\x07\x99\x07\x07\x07"
    "\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39\x07\x07\x9A\x07\x07\x07"
};


unsigned char
cp_437_to_500[] = {
    "\x00\x01\x02\x03\x37\x2D\x2E\x2F\x16\x05\x15\x0B\x0C\x0D\x0E\x0F"
    "\x10\x11\x12\x13\x3C\x3D\x32\x26\x18\x19\x3F\x27\x22\x1D\x1E\x1F"
    "\x40\x4F\x7F\x7B\x5B\x6C\x50\x7D\x4D\x5D\x5C\x4E\x6B\x60\x4B\x61"
    "\xF0\xF1\xF2\xF3\xF4\xF5\xF6\xF7\xF8\xF9\x7A\x5E\x4C\x7E\x6E\x6F"
    "\x7C\xC1\xC2\xC3\xC4\xC5\xC6\xC7\xC8\xC9\xD1\xD2\xD3\xD4\xD5\xD6"
    "\xD7\xD8\xD9\xE2\xE3\xE4\xE5\xE6\xE7\xE8\xE9\x4A\xE0\x5A\x5F\x6D"
    "\x79\x81\x82\x83\x84\x85\x86\x87\x88\x89\x91\x92\x93\x94\x95\x96"
    "\x97\x98\x99\xA2\xA3\xA4\xA5\xA6\xA7\xA8\xA9\xC0\xBB\xD0\xA1\x07"
    "\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F"
    "\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F"
    "\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F"
    "\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F"
    "\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F"
    "\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F"
    "\x3F\x59\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F"
    "\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x3F\x90\x3F\x3F\x3F\x3F\xEA\x3F\xFF"
};


unsigned char
cp_500_to_437[] = {
    "\x00\x01\x02\x03\x07\x09\x07\x7F\x07\x07\x07\x0B\x0C\x0D\x0E\x0F"
    "\x10\x11\x12\x13\x07\x0A\x08\x07\x18\x19\x07\x07\x07\x07\x07\x07"
    "\x07\x07\x1C\x07\x07\x0A\x17\x1B\x07\x07\x07\x07\x07\x05\x06\x07"
    "\x07\x07\x16\x07\x07\x07\x07\x04\x07\x07\x07\x07\x14\x15\x07\x1A"
    "\x20\xFF\x83\x84\x85\xA0\x07\x86\x87\xA4\x5B\x2E\x3C\x28\x2B\x21"
    "\x26\x82\x88\x89\x8A\xA1\x8C\x07\x8D\xE1\x5D\x24\x2A\x29\x3B\x5E"
    "\x2D\x2F\x07\x8E\x07\x07\x07\x8F\x80\xA5\x07\x2C\x25\x5F\x3E\x3F"
    "\x07\x90\x07\x07\x07\x07\x07\x07\x70\x60\x3A\x23\x40\x27\x3D\x22"
    "\x07\x61\x62\x63\x64\x65\x66\x67\x68\x69\xAE\xAF\x07\x07\x07\xF1"
    "\xF8\x6A\x6B\x6C\x6D\x6E\x6F\x70\x71\x72\xA6\xA7\x91\x07\x92\x07"
    "\xE6\x7E\x73\x74\x75\x76\x77\x78\x79\x7A\xAD\xAB\x07\x07\x07\x07"
    "\x9B\x9C\x9D\xFA\x07\x07\x07\xAC\xAB\x07\xAA\x7C\x07\x07\x07\x07"
    "\x7B\x41\x42\x43\x44\x45\x46\x47\x48\x49\x07\x93\x94\x95\xA2\x07"
    "\x7D\x4A\x4B\x4C\x4D\x4E\x4F\x50\x51\x52\x07\x96\x81\x97\xA3\x98"
    "\x5C\xF6\x53\x54\x55\x56\x57\x58\x59\x5A\xFD\x07\x99\x07\x07\x07"
    "\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39\x07\x07\x9A\x07\x07\x07"
};

unsigned char
cp_850_to_273[] = {
    "\x00\x01\x02\x03\x37\x2D\x2E\x2F\x16\x05\x25\x0B\x0C\x0D\x0E\x0F"
    "\x10\x11\x12\x13\x3C\x3D\x32\x26\x18\x19\x3F\x27\x1C\x1D\x1E\x1F"
    "\x40\x4F\x7F\x7B\x5B\x6C\x50\x7D\x4D\x5D\x5C\x4E\x6B\x60\x4B\x61"
    "\xF0\xF1\xF2\xF3\xF4\xF5\xF6\xF7\xF8\xF9\x7A\x5E\x4C\x7E\x6E\x6F"
    "\xB5\xC1\xC2\xC3\xC4\xC5\xC6\xC7\xC8\xC9\xD1\xD2\xD3\xD4\xD5\xD6"
    "\xD7\xD8\xD9\xE2\xE3\xE4\xE5\xE6\xE7\xE8\xE9\x63\xEC\xFC\x5F\x6D"
    "\x79\x81\x82\x83\x84\x85\x86\x87\x88\x89\x91\x92\x93\x94\x95\x96"
    "\x97\x98\x99\xA2\xA3\xA4\xA5\xA6\xA7\xA8\xA9\x43\xBB\xDC\x59\x07"
    "\x68\xD0\x51\x42\xC0\x44\x47\x48\x52\x53\x54\x57\x56\x58\x4A\x67"
    "\x71\x9C\x9E\xCB\x6A\xCD\xDB\xDD\xDF\xE0\x5A\x70\xB1\x80\xBF\xFF"
    "\x45\x55\xCE\xDE\x49\x69\x9A\x9B\xAB\xAF\xBA\xB8\xB7\xAA\x8A\x8B"
    "\x2B\x2C\x09\x21\x28\x65\x62\x64\xB4\x38\x31\x34\x33\xB0\xB2\x24"
    "\x22\x17\x29\x06\x20\x2A\x46\x66\x1A\x35\x08\x39\x36\x30\x3A\x9F"
    "\x8C\xAC\x72\x73\x74\x0A\x75\x76\x77\x23\x15\x14\x04\xCC\x78\x3B"
    "\xEE\xA1\xEB\xED\xCF\xEF\xA0\x8E\xAE\xFE\xFB\xFD\x8D\xAD\xBC\xBE"
    "\xCA\x8F\x1B\xB9\xB6\x7C\xE1\x9D\x90\xBD\xB3\xDA\xFA\xEA\x3E\x41"
    };

unsigned char
cp_273_to_850[] = {
    "\x00\x01\x02\x03\xDC\x09\xC3\x7F\xCA\xB2\xD5\x0B\x0C\x0D\x0E\x0F"
    "\x10\x11\x12\x13\xDB\xDA\x08\xC1\x18\x19\xC8\xF2\x1C\x1D\x1E\x1F"
    "\xC4\xB3\xC0\xD9\xBF\x0A\x17\x1B\xB4\xC2\xC5\xB0\xB1\x05\x06\x07"
    "\xCD\xBA\x16\xBC\xBB\xC9\xCC\x04\xB9\xCB\xCE\xDF\x14\x15\xFE\x1A"
    "\x20\xFF\x83\x7B\x85\xA0\xC6\x86\x87\xA4\x8E\x2E\x3C\x28\x2B\x21"
    "\x26\x82\x88\x89\x8A\xA1\x8C\x8B\x8D\x7E\x9A\x24\x2A\x29\x3B\x5E"
    "\x2D\x2F\xB6\x5B\xB7\xB5\xC7\x8F\x80\xA5\x94\x2C\x25\x5F\x3E\x3F"
    "\x9B\x90\xD2\xD3\xD4\xD6\xD7\xD8\xDE\x60\x3A\x23\xF5\x27\x3D\x22"
    "\x9D\x61\x62\x63\x64\x65\x66\x67\x68\x69\xAE\xAF\xD0\xEC\xE7\xF1"
    "\xF8\x6A\x6B\x6C\x6D\x6E\x6F\x70\x71\x72\xA6\xA7\x91\xF7\x92\xCF"
    "\xE6\xE1\x73\x74\x75\x76\x77\x78\x79\x7A\xAD\xA8\xD1\xED\xE8\xA9"
    "\xBD\x9C\xBE\xFA\xB8\x40\xF4\xAC\xAB\xF3\xAA\x7C\xEE\xF9\xEF\x9E"
    "\x84\x41\x42\x43\x44\x45\x46\x47\x48\x49\xF0\x93\xDD\x95\xA2\xE4"
    "\x81\x4A\x4B\x4C\x4D\x4E\x4F\x50\x51\x52\xFB\x96\x7D\x97\xA3\x98"
    "\x99\xF6\x53\x54\x55\x56\x57\x58\x59\x5A\xFD\xE2\x5C\xE3\xE0\xE5"
    "\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39\xFC\xEA\x5D\xEB\xE9\x9F"
    };

CPCONV cpconv[] = {
    { "default",  ascii_to_ebcdic, ebcdic_to_ascii },
    { "437/037",  cp_437_to_037, cp_037_to_437 },
    { "437/500",  cp_437_to_500, cp_500_to_437 },
    { "850/273",  cp_850_to_273, cp_273_to_850 },
    { NULL,       ascii_to_ebcdic, ebcdic_to_ascii } };


void set_codepage(char *name)
{
    for(sysblk.codepage = cpconv; 
        sysblk.codepage->name && strcasecmp(sysblk.codepage->name,name);
        sysblk.codepage++);

    if(!sysblk.codepage->name)
        logmsg(_("HHC050I CodePage conversion table %s is not defined\n"),name);
}
