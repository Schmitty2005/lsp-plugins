/*
 * Copyright (C) 2020 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2020 Vladimir Sadovnikov <sadko4u@gmail.com>
 *
 * This file is part of lsp-plugins
 * Created on: 24 марта 2016 г.
 *
 * lsp-plugins is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * lsp-plugins is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with lsp-plugins. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef CORE_FADE_H_
#define CORE_FADE_H_

#include <core/types.h>

namespace lsp
{
    /** Fade-in (with range check)
     *
     * @param dst destination buffer
     * @param src source buffer
     * @param fade_len length of fade (in elements)
     * @param buf_len length of the buffer
     */
    void fade_in(float *dst, const float *src, size_t fade_len, size_t buf_len);

    /** Fade-out (with range check)
     *
     * @param dst destination buffer
     * @param src source buffer
     * @param fade_len length of fade (in elements)
     * @param buf_len length of the buffer
     */
    void fade_out(float *dst, const float *src, size_t fade_len, size_t buf_len);
}

#endif /* CORE_FADE_H_ */
