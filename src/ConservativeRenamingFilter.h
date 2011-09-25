/*
 * This file is part of synconv.
 *
 * © 2011 Fernando Tarlá Cardoso Lemos
 *
 * synconv is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * synconv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with synconv.  If not, see <http://www.gnu.org/licenses/>.
 * Refer to the LICENSE file for licensing information.
 *
 */

#ifndef CONSERVATIVE_RENAMING_FILTER_H
#define CONSERVATIVE_RENAMING_FILTER_H

#include "RenamingFilter.h"

class ConservativeRenamingFilter : public RenamingFilter
{
    private:
        bool is_character_allowed(wchar_t c) const
        {
            if (c >= L'a' && c <= L'z') return true;
            if (c >= L'A' && c <= L'Z') return true;
            if (c >= L'0' && c <= L'9') return true;

            static wchar_t allowed_chars[] = {
                L' ', L'%', L'-', L'_', L'@', L'~', L'`', L'!',
                L'(', L')', L'{', L'}', L'^', L'#', L'&', L'.'
            };
            for (unsigned int i = 0; i < sizeof(allowed_chars) / sizeof(wchar_t); ++i) {
                if (c == allowed_chars[i])
                    return true;
            }

            return false;
        }

        wchar_t replacement_character(wchar_t c) const
        {
            return L'_';
        }
};

#endif
