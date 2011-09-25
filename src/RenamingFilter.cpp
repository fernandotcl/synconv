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

#include <boost/foreach.hpp>

#include "RenamingFilter.h"

std::wstring RenamingFilter::filter(const std::wstring &path_component) const
{
    std::wstring new_component;
    new_component.reserve(path_component.size());

    BOOST_FOREACH(wchar_t c, path_component)
        new_component += is_character_allowed(c) ? c : replacement_character(c);

    if (new_component.empty())
        new_component += replacement_character(L'A');

    return new_component;
}
