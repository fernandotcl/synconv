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

#ifndef ENCODER_H
#define ENCODER_H

#include <boost/foreach.hpp>
#include <list>
#include <string>

extern "C" {
#include <pipeline.h>
}

class Encoder
{
    public:
        Encoder() {};
        virtual ~Encoder() {};

        virtual void enter_encoder_pipeline(pipeline *p) = 0;

        void add_extra_option(const std::string &option) { m_extra_options.push_back(option); }

    protected:
        void add_extra_options(pipecmd *cmd)
        {
            BOOST_FOREACH(const std::string &option, m_extra_options)
                pipecmd_arg(cmd, option.c_str());
        }

        std::list<std::string> m_extra_options;
};

#endif
