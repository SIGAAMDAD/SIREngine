// copyright SIGAAMDAD 2023 (this is an implementation of std::ifstream, std::ofstream, and std::fstream so that i can use the eastl with other deps)

#ifndef EASTL_FSTREAM_H
#define EASTL_FSTREAM_H

#include <EASTL/internal/config.h>

#if defined(EA_PRAGMA_ONCE_SUPPORTED)
	#pragma once // Some compilers (e.g. VC++) benefit significantly from using this. We've measured 3-4% build speed improvements in apps as a result.
#endif

namespace eastl
{
    namespace ios
    {
        enum openmode {
            binary = 0,
            ate,
            in,
            out,
            trunc
        };
        enum seekdir {
            beg = 0,
            end,
            cur
        };
    };

    typedef size_t streamsize;
#ifdef _MSVC_VER
    typedef __off_t streamoffset;
#else
    typedef off_t streamoffset;
#endif

    template<char CharT>
    class basic_fstream
    {
    public:
        typedef CharT char_type;
        typedef size_t size_type;
        typedef int64_t int_type;
        typedef ::fpos_t pos_type;
        typedef streamoffset off_type;

        typedef eastl::vector<char_type> filebuf_type;
        typedef eastl::basic_fstream<char_type> fstream_type;
    private:
        filebuf_type mFilebuf;
        FILE *mpStream;
        pos_type mPos;
    public:
        EA_CPP14_CONSTEXPR inline basic_fstream()
            : mpStream(NULL), mPos(0)
        {}
        inline basic_fstream(const eastl::string& filePath, eastl::ios::openmode mode)
        {
            if (mode & eastl::ios::in)
        }
        inline ~basic_fstream()
        {
            if (mpStream)
                ::fclose(mpStream);
        }

        inline pos_type seekg(size_t whence, eastl::ios::seekdir offset)
        {
            
        }
    };

} // namespace eastl

#endif // Header include guard