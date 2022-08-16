
/**
 * \section Purpose
 *
 * Implementation of several methods defined in string.h, for reducing the
 * memory footprint when using them (since the whole libc.o file gets included
 * even when using a single method).
 *
 * \section Usage
 *
 * Add string.c to the list of files to compile for the project. This will
 * automatically replace standard libc methods by the custom ones.
 */

/*----------------------------------------------------------------------------
 *        Headers
 *----------------------------------------------------------------------------*/

#include <string.h>

/*----------------------------------------------------------------------------
 *        Exported functions
 *----------------------------------------------------------------------------*/

/**
 * \brief Copies data from a source buffer into a destination buffer. The two buffers
 * must NOT overlap. Returns the destination buffer.
 *
 * \param pDestination  Destination buffer.
 * \param pSource  Source buffer.
 * \param num  Number of bytes to copy.
 */
void * memcpy(void *pDestination, const void *pSource, size_t num)
{
    unsigned char *pByteDestination;
    unsigned char *pByteSource;
    unsigned int *pAlignedSource = (unsigned int *) pSource;
    unsigned int *pAlignedDestination = (unsigned int *) pDestination;

    // in 32bit machine, the start address is always aligned with 4 bytes border
    /* If num is more than 4 bytes, and both dest. and source are aligned, */
    /* then copy dwords */
    if ((((unsigned int) pAlignedDestination & 0x3) == 0)
        && (((unsigned int) pAlignedSource & 0x3) == 0)
        && (num >= 4)) {

        while (num >= 4) {

            *pAlignedDestination++ = *pAlignedSource++;
            num -= 4;
        }
    }

    /* Copy remaining bytes */
    pByteDestination = (unsigned char *) pAlignedDestination;
    pByteSource = (unsigned char *) pAlignedSource;
    while (num--) {

        *pByteDestination++ = *pByteSource++;
    }

    return pDestination;
}

/**
 * \brief Fills a memory region with the given value. Returns a pointer to the
 * memory region.
 *
 * \param pBuffer  Pointer to the start of the memory region to fill
 * \param value    Value to fill the region with
 * \param num      Size to fill in bytes
 */
void * memset(void *pBuffer, int value, size_t num)
{
    unsigned char *pByteDestination;
    unsigned int  *pAlignedDestination = (unsigned int *) pBuffer;
    unsigned int  alignedValue = (value << 24) | (value << 16) | (value << 8) | value;

    // in 32bit machine, the start address is always aligned with 4 bytes border
    /* Set words if possible */
    if ((((unsigned int) pAlignedDestination & 0x3) == 0) && (num >= 4)) {
        while (num >= 4) {
            *pAlignedDestination++ = alignedValue;
            num -= 4;
        }
    }
    /* Set remaining bytes */
    pByteDestination = (unsigned char *) pAlignedDestination;
    while (num--) {
        *pByteDestination++ = value;
    }
    return pBuffer;
}

/**
 * \brief Search a character in the given string.
 *
 * \param pString   Pointer to the start of the string to search.
 * \param character The character to find.
 *
 * \return Returns a pointer to the character location.
 */
char * strchr(const char *pString, int character)
{
    char * p = (char *)pString;
    char   c = character & 0xFF;

    while(*p != c) {
        if (*p == 0) {
            return 0;
        }
        p++;
    }
    return p;
}

/**
 * \brief Get the length of a given string
 *
 * \param pString Pointer to the start of the string.
 *
 * \return Return the length of a given string.
 */
size_t strlen(const char *pString)
{
    unsigned int length = 0;

    while(*pString++ != 0) {
        length++;
    }
    return length;
}


/**
 * \brief Search a character backword from the end of given string.
 *
 * \param pString   Pointer to the start of the string to search.
 * \param character The character to find.
 *
 * \return Returns a pointer to the character location.
 */
char * strrchr(const char *pString, int character)
{
    char *p = 0;

    while(*pString != 0) {
        if (*pString++ == character) {
            p = (char*)pString;
        }
    }
    return p;
}

/**
 * \breif Copy from source string to destination string.
 *
 * \param pDestination Pointer to the destination string.
 * \param pSource      Pointer to the source string.
 *
 * \return Return a pointer to the destination string.
 */
char * strcpy(char *pDestination, const char *pSource)
{
    char *pSaveDest = pDestination;

    for(; (*pDestination = *pSource) != 0; ++pSource, ++pDestination);
    return pSaveDest;
}

/**
 * \brief Compare the first specified bytes of 2 given strings
 *
 * \param pString1 Pointer to the start of the 1st string.
 * \param pString2 Pointer to the start of the 2nd string.
 * \param count    Number of bytes that should be compared.
 *
 * \return Return 0 if equals,
 * Return >0 if 1st string > 2nd string
 * Return <0 if 1st string < 2nd string
 */
int strncmp(const char *pString1, const char *pString2, size_t count)
{
    int r;

    while(count) {
        r = *pString1 - *pString2;
        if (r == 0) {
            if (*pString1 == 0) {
                break;
            }
            pString1++;
            pString2++;
            count--;
            continue;
        }
        return r;
    }
    return 0;
}

/**
 * \brief Copy the first number of bytes from source string to destination string
 *
 * \param pDestination Pointer to the start of destination string.
 * \param pSource      Pointer to the start of the source string.
 * \param count        Number of bytes that should be copied.
 *
 * \return Return the pointer to the destination string.
 */
char * strncpy(char *pDestination, const char *pSource, size_t count)
{
    char *pSaveDest = pDestination;

    while (count) {
        *pDestination = *pSource;
        if (*pSource == 0) {
            break;
        }
        pDestination++;
        pSource++;
        count--;
    }
    return pSaveDest;
}

