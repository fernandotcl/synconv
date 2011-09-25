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

#ifndef RENAMING_FILTER_H
#define RENAMING_FILTER_H

#include <string>

class RenamingFilter
{
    public:
        std::wstring filter(const std::wstring &path_component) const;

    private:
        virtual bool is_character_allowed(wchar_t c) const = 0;
        virtual wchar_t replacement_character(wchar_t c) const = 0;
};

#endif
