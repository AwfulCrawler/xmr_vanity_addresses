// Author: AwfulCrawler (2017)
//
// Parts of this file are orignally copyright (c) 2014-2016, The Monero Project
// and copyright (c) 2012-2013 The Cryptonote developers
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

//message_writer class taken from simplewallet.cpp in monero repository

#pragma once

#include <iostream>
#include <sstream>
#include "console_handler.h"

//------------------------------------------------------------------------------
//CONSTANTS
#define DEFAULT_MIN_START_POS         1
#define DEFAULT_MAX_START_POS         2
#define DEFAULT_MIN_WORD_LENGTH       4
#define DEFAULT_MAX_WORD_LENGTH       11
#define DEFAULT_NUM_THREADS           4

#define DEFAULT_SEARCH_LENGTH         6

#define LINE_WIDTH_LIMIT              80  //For outputting vanity search results

#define TESTNET                       true
#define RPC_CONNECTION_TIMEOUT        200000

#define ADDRESS_BASE58_PREFIX_XMR      18   //4[1-9|A|B] etc.
#define ADDRESS_BASE58_PREFIX_XMRTEST  53   //[9|A][something] etc.
#define ADDRESS_BASE58_PREFIX_AEON     0xB2 //Wm[r|s|t|u] etc.

//A map.. so we can provide arguments
std::map<std::string, uint64_t> prefix_map
{
  {"XMR"      , ADDRESS_BASE58_PREFIX_XMR},
  {"XMR_TEST" , ADDRESS_BASE58_PREFIX_XMRTEST},
  {"AEON"     , ADDRESS_BASE58_PREFIX_AEON},
};


//------------------------------------------------------------------------------

//-------------------------------------------------------------------------------
class message_writer
  {
  public:
    message_writer(epee::log_space::console_colors color = epee::log_space::console_color_default, bool bright = false,
      std::string&& prefix = std::string())
      : m_flush(true)
      , m_color(color)
      , m_bright(bright)
    {
      m_oss << prefix;
    }

    message_writer(message_writer&& rhs)
      : m_flush(std::move(rhs.m_flush))
#if defined(_MSC_VER)
      , m_oss(std::move(rhs.m_oss))
#else
      // GCC bug: http://gcc.gnu.org/bugzilla/show_bug.cgi?id=54316
      , m_oss(rhs.m_oss.str(), ios_base::out | ios_base::ate)
#endif
      , m_color(std::move(rhs.m_color))
    {
      rhs.m_flush = false;
    }

    template<typename T>
    std::ostream& operator<<(const T& val)
    {
      m_oss << val;
      return m_oss;
    }

    ~message_writer()
    {
      if (m_flush)
      {
        m_flush = false;

        if (epee::log_space::console_color_default == m_color)
        {
          std::cout << m_oss.str();
        }
        else
        {
          epee::log_space::set_console_color(m_color, m_bright);
          std::cout << m_oss.str();
          epee::log_space::reset_console_color();
        }
        //std::cout << std::endl;
      }
    }

  private:
    message_writer(message_writer& rhs);
    message_writer& operator=(message_writer& rhs);
    message_writer& operator=(message_writer&& rhs);

  private:
    bool m_flush;
    std::stringstream m_oss;
    epee::log_space::console_colors m_color;
    bool m_bright;
  };
//-------------------------------------------------------------------------------
message_writer success_msg_writer(bool color = false)
{
  return message_writer(color ? epee::log_space::console_color_green : epee::log_space::console_color_green, false, std::string());
}
//-------------------------------------------------------------------------------
message_writer fail_msg_writer()
{
  return message_writer(epee::log_space::console_color_red, true, "Error: ");
}
