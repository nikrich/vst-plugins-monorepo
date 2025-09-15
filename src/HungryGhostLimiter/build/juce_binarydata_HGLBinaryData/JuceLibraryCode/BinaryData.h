/* =========================================================================================

   This is an auto-generated file: Any edits you make may be overwritten!

*/

#pragma once

namespace BinaryData
{
    extern const char*   logo_png;
    const int            logo_pngSize = 44001;

    extern const char*   logoimg_png;
    const int            logoimg_pngSize = 150382;

    extern const char*   mkfinal_png;
    const int            mkfinal_pngSize = 2450945;

    extern const char*   background03_png;
    const int            background03_pngSize = 431729;

    extern const char*   sbfinal_png;
    const int            sbfinal_pngSize = 13785222;

    extern const char*   slfinal_png;
    const int            slfinal_pngSize = 956000;

    extern const char*   _001_png;
    const int            _001_pngSize = 2947;

    extern const char*   _002_png;
    const int            _002_pngSize = 7262;

    extern const char*   MontserratItalicVariableFont_wght_ttf;
    const int            MontserratItalicVariableFont_wght_ttfSize = 701156;

    extern const char*   MontserratVariableFont_wght_ttf;
    const int            MontserratVariableFont_wght_ttfSize = 688600;

    // Number of elements in the namedResourceList and originalFileNames arrays.
    const int namedResourceListSize = 10;

    // Points to the start of a list of resource names.
    extern const char* namedResourceList[];

    // Points to the start of a list of resource filenames.
    extern const char* originalFilenames[];

    // If you provide the name of one of the binary resource variables above, this function will
    // return the corresponding data and its size (or a null pointer if the name isn't found).
    const char* getNamedResource (const char* resourceNameUTF8, int& dataSizeInBytes);

    // If you provide the name of one of the binary resource variables above, this function will
    // return the corresponding original, non-mangled filename (or a null pointer if the name isn't found).
    const char* getNamedResourceOriginalFilename (const char* resourceNameUTF8);
}
