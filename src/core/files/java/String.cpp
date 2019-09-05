/*
 * String.cpp
 *
 *  Created on: 29 авг. 2019 г.
 *      Author: sadko
 */

#include <core/files/java/defs.h>
#include <core/files/java/String.h>

namespace lsp
{
    namespace java
    {
        const char *String::CLASS_NAME  = "java.lang.String";

        String::String(): Object(CLASS_NAME)
        {
        }
        
        String::~String()
        {
        }

        status_t String::to_string(LSPString *dst)
        {
            if (!dst->fmt_append_ascii("%p = (String) \"", this))
                return STATUS_NO_MEM;
            if (!dst->append(&sString))
                return STATUS_NO_MEM;
            return (dst->append('\"')) ? STATUS_OK : STATUS_NO_MEM;
        }
    
    } /* namespace java */
} /* namespace lsp */
