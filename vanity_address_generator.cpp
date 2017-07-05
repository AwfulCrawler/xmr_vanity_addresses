// Author: AwfulCrawler (2017)
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


#include "trim_account.h"
#include "vanity_address_generator.h"
#include "logo_monero.h"
#include "aeon-words.h"

#include <thread>
#include <memory>
#include <fstream>
#include <unordered_map>
#include <ctype.h>
#include <string>
#include <stdexcept>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/lock_guard.hpp>

#include "mnemonics/electrum-words.h"

//Benchmarking
#include <chrono>
typedef std::chrono::high_resolution_clock Clock;

//GLOBAL VARIABLES

//------------VANITY SEARCH----------------
std::ofstream my_ostream;
std::unordered_map<std::string, std::vector<std::string>> word_list;
std::unordered_map<std::string, std::vector<std::string>> found_words;

boost::mutex my_output_lock;
std::vector<std::thread> search_threads;
bool search_active=false;

namespace options
{
  bool        show_success_msg     {false};
  uint32_t    min_start_pos        {DEFAULT_MIN_START_POS};
  uint32_t    max_start_pos        {DEFAULT_MAX_START_POS};
  uint32_t    search_word_length   {DEFAULT_SEARCH_LENGTH};
  uint64_t    address_prefix       {ADDRESS_BASE58_PREFIX_XMR};
  std::string address_prefix_label {"XMR"};
}
//-----------------------------------------

epee::console_handlers_binder m_cmd_binder;

//--------------------------------------------------------------------------------
//
//LOAD WORDS AND SEARCHING FUNCTIONS INCLUDING SEARCH_THREAD FUNCTION
//
//--------------------------------------------------------------------------------
bool load_word_list(const std::string& word_filename)
{
  std::string line;
  std::ifstream word_list_file (word_filename);
  if (word_list_file.is_open())
  {
    word_list.clear();
    while (getline(word_list_file, line))
    {
      boost::trim_right(line);
      boost::to_upper(line);
      if (line.length() == 0) continue;

      if (line.find("'")    == std::string::npos
          && line.find("/") == std::string::npos
          && line.find("&") == std::string::npos
          && line.length()  >= options::search_word_length)
      {
        std::string search_string = line.substr(0, options::search_word_length);
        auto search_results = word_list.find(search_string);
        if (search_results == word_list.end())
        {
          auto ret_val = word_list.insert(std::pair<std::string, std::vector<std::string>>(search_string, std::vector<std::string>()));
          search_results = ret_val.first;
        }
        auto & sub_list = search_results->second;
        sub_list.push_back(line);
      }
    }
    word_list_file.close();
    return true;
  }
  else{
    std::cout << "Unable to open file " << word_filename << std::endl;
    return false;
  }
}

//--------------------------------------------------------------------------------

void save_data(const std::string& found_word, const std::string& address_string, trim_account& m_account)
{
  boost::lock_guard<boost::mutex> lock(my_output_lock);

  auto search_results = found_words.find(found_word);
  if (search_results == found_words.end())
  {
    auto ret_val   = found_words.insert(std::pair<std::string, std::vector<std::string>>(found_word, std::vector<std::string>()));
    search_results = ret_val.first;
  }
  auto & matched_addresses = search_results->second;
  matched_addresses.push_back(address_string);

  std::string electrum_words;
  if (options::address_prefix == ADDRESS_BASE58_PREFIX_AEON)
  {
    crypto::AeonWords::bytes_to_words(m_account.get_raw_private_spend_key(), electrum_words);
  }
  else
  {
    crypto::ElectrumWords::bytes_to_words(m_account.get_raw_private_spend_key(), electrum_words, "English");
  }

  //Output to a filestream which we create with the constructor
  my_ostream << "------------------------------------" << std::endl
             << "WORD:     " << found_word << std::endl
             << "ADDRESS:  " << address_string << std::endl
             << "SPENDKEY: " << m_account.get_private_spend_key() << std::endl
             << "VIEWKEY:  " << m_account.get_private_view_key() << std::endl
             << electrum_words << std::endl
             << "------------------------------------" << std::endl;
  my_ostream.flush();

  if (options::show_success_msg)
  {
    success_msg_writer() << "\rMatch found for \"" << found_word << "\": " << address_string << std::endl;
    m_cmd_binder.print_prompt();
  }
}

//--------------------------------------------------------------------------------

void thread_safe_print(const std::string & a_string)
{
  boost::lock_guard<boost::mutex> lock(my_output_lock);
  std::cout << a_string << std::endl;
}

//--------------------------------------------------------------------------------

void search_thread_single_word( const uint32_t thread_num, const std::string search_word )
{
  trim_account m_account;
  std::string upper_search_word = boost::to_upper_copy(search_word);
  size_t word_length = search_word.length();

  uint64_t num_searches = 0;
  auto start_time = Clock::now();

  while(search_active)
  {
    m_account.increment_keys();
    std::string public_address_string = m_account.get_public_address_str(options::address_prefix);
    std::string upper_address         = boost::to_upper_copy(public_address_string);

    for (uint32_t start_pos=options::min_start_pos; start_pos<=options::max_start_pos; start_pos++)
    {
      std::string trimmed_address       = upper_address.substr(start_pos, word_length);
      if (trimmed_address == upper_search_word)
      {
        save_data(trimmed_address, public_address_string, m_account);
        m_account.random_keys();
        break;
      }
    }
    num_searches += 1;
  }

  //After search stops print some stats for the thread
  auto end_time = Clock::now();

  double duration           = ((double) std::chrono::duration_cast<std::chrono::milliseconds>(end_time-start_time).count())/1000;
  double addresses_per_sec  = (double) num_searches / duration;

  std::stringstream ss;
  ss << "Thread [" << thread_num << "]: \n"
     << num_searches      << " Addresses Checked\n"
     << duration          << " Seconds\n"
     << addresses_per_sec << " Addresses / Sec on Average" << std::endl;
  thread_safe_print(ss.str());
}

//--------------------------------------------------------------------------------

void search_thread(const uint32_t thread_num)
{
  trim_account m_account;
  std::stringstream ss;

  uint64_t num_searches = 0;
  auto start_time = Clock::now();

  uint32_t word_length = options::search_word_length;

  while(search_active)
  {
    m_account.increment_keys();
    std::string public_address_string = m_account.get_public_address_str(options::address_prefix);
    std::string upper_address         = boost::to_upper_copy(public_address_string);

    //--------------------------------
    bool found_matches = false;
    for (uint32_t start_pos=options::min_start_pos; start_pos<=options::max_start_pos; start_pos++)
    {
      std::string trimmed_address = upper_address.substr(start_pos, word_length);
      auto search_results = word_list.find(trimmed_address);
      if ( search_results != word_list.end())
      {
        for (const std::string & x : search_results->second)
        {
          if (upper_address.compare(start_pos, x.length(), x) == 0)
          {
            save_data(x, public_address_string, m_account);
            found_matches = true;
          }
        }
      }
    }
    num_searches += 1;
    if (found_matches) m_account.random_keys();
    //--------------------------------
  }

  //After search stops print some stats for the thread
  auto end_time = Clock::now();

  double duration           = ((double) std::chrono::duration_cast<std::chrono::milliseconds>(end_time-start_time).count())/1000;
  double addresses_per_sec  = (double) num_searches / duration;

  ss << "Thread [" << thread_num << "]: \n"
     << num_searches      << " Addresses Checked\n"
     << duration          << " Seconds\n"
     << addresses_per_sec << " Addresses / Sec on Average" << std::endl;
  thread_safe_print(ss.str());
}

//--------------------------------------------------------------------------------

//--------------------------------------------------------------------------------
//
//COMMANDS
//
//--------------------------------------------------------------------------------
bool start_search(const std::vector<std::string> &args)
{
  int  search_num_threads;
  bool single_word_search = false;
  std::string search_word;

  if (search_active)
  {
    std::cout << "Search is already active.  No action taken." << std::endl;
    return true;
  }

  if (args.size() < 2)
  {
    fail_msg_writer() << "Need <word file> or <word> and <output file> arguments" << std::endl;
    return true;
  }
  else
  {
    try
    {
      bool success = load_word_list(args[0]);
      if (!success)
      {
        //fail_msg_writer() << "could not load word list file " << args[0] << std::endl;
        //return true;
        std::cout << "Using \"" << args[0] << "\" as a single search word..." << std::endl;
        single_word_search = true;
        search_word = args[0];
      }

      my_ostream.open(args[1]);
      if (!my_ostream.is_open())
      {
        fail_msg_writer() << "could not open " << args[1] << " for writing" << std::endl;
        return true;
      }
    }
    catch (std::exception &e)
    {
      fail_msg_writer() << "exception while opening files: " << e.what() << std::endl;
      return true;
    }
    if (args.size() > 2)
    {
      try
      {
        search_num_threads = boost::lexical_cast<int>(args[2]);
      }
      catch(boost::bad_lexical_cast& e)
      {
        std::cout << "Could not parse number of threads" << std::endl
                  << "Using default number of threads (" << DEFAULT_NUM_THREADS << ")" << std::endl;
        search_num_threads = DEFAULT_NUM_THREADS;
      }
      if (search_num_threads < 0){
        std::cout << "Positive number of threads required" << std::endl;
        return true;
      }
    }
    else
    {
      search_num_threads = DEFAULT_NUM_THREADS;
    }
  }
  std::cout << "Starting vanity search with " << search_num_threads << " threads..." << std::endl;
  search_active=true;

  if (single_word_search)
  {
    for (int i=0;i<search_num_threads;i++) search_threads.push_back(std::thread(search_thread_single_word, i, search_word));
  }
  else
  {
    for (int i=0;i<search_num_threads;i++) search_threads.push_back(std::thread(search_thread,i));
  }
  return true;
}

//--------------------------------------------------------------------------------

bool stop_search(const std::vector<std::string> &args)
{
  if (!search_active)
  {
    std::cout << "Search is not active.  No action taken." << std::endl;
    return true;
  }
  std::cout << "Stopping Vanity Search..." << std::endl;
  search_active=false;
  for (size_t i=0;i<search_threads.size(); i++) search_threads[i].join();
  search_threads.clear(); //Delete all threads from memory.
  std::cout << "Vanity Search Stopped" << std::endl;
  my_ostream.close();
  return true;
}

//--------------------------------------------------------------------------------

bool show_results(const std::vector<std::string> &args)
{
  int  length_threshold;
  char first_letter_filter;
  bool filter_by_length=false;
  bool filter_by_letter=false;
  //If arguments are supplied in the form of single letters, only show results beginning with that letter.

  if (args.size() == 1)
  {
    try
    {
      length_threshold = boost::lexical_cast<int>(args[0]);
      filter_by_length = true;
      std::cout << "Showing all words with length >= "
                << length_threshold << std::endl
                << "------------------------" << std::endl;
    }
    catch(boost::bad_lexical_cast& e)
    {
      if (args[0].length() > 1)
      {
        fail_msg_writer() << "Expected single letter or number or both" << std::endl;
        return true;
      }
      else
      {
        first_letter_filter = args[0][0];
        filter_by_letter = true;
        std::cout << "Showing all words starting with \""
                  << first_letter_filter << "\"" << std::endl
                  << "------------------------" << std::endl;
      }
    }
  }
  else if (!args.empty())
  {
    filter_by_letter = true;
    filter_by_length = true;
    try{
      length_threshold    = boost::lexical_cast<int>(args[0]);
      if (args[1].length() == 1)
        first_letter_filter = args[1][0];
      else
      {
        fail_msg_writer() << "Expected a number and a single letter in either order" << std::endl;
        return true;
      }
    }
    catch(boost::bad_lexical_cast& e){
      try{
        length_threshold    = boost::lexical_cast<int>(args[1]);
        if (args[0].length() == 1)
          first_letter_filter = args[0][0];
        else
        {
          fail_msg_writer() << "Expected a number and a single letter in either order" << std::endl;
          return true;
        }
      }
      catch(boost::bad_lexical_cast& e){
        fail_msg_writer() << "Expected a number and a single letter in either order" << std::endl;
        return true;
      }
    }
    std::cout << "Showing all words starting with \"" << first_letter_filter
              << "\" with length >= " << length_threshold << std::endl
              << "------------------------" << std::endl;
  }
  else
  {
    std::cout << "Showing all found words:" << std::endl
              << "------------------------" << std::endl;
  }

  std::vector<std::string> found_words_vec;
  for (const auto & x : found_words)
  {
    if ((filter_by_letter && x.first[0] != toupper(first_letter_filter))
        || (filter_by_length && x.first.length() < length_threshold)) continue;

    found_words_vec.push_back(x.first);
  }
  std::sort(found_words_vec.begin(), found_words_vec.end());
  int line_width = 0;
  for (const std::string & x : found_words_vec)
  {
    std::cout << x << " ";
    line_width += x.length()+1;
    if (line_width > LINE_WIDTH_LIMIT)
    {
      std::cout << std::endl;
      line_width = 0;
    }
  }
  if (line_width != 0) std::cout << std::endl;
  std::cout << "------------------------" << std::endl;

  return true;
}

//-------------------------------------------------------------------------------

bool show_addresses(const std::vector<std::string> &args)
{
  if (args.empty())
  {
    fail_msg_writer() << "Expected search word" << std::endl;
    return true;
  }

  std::cout << "Showing results for \"" << args[0] << "\"" << "\n"
            << "-------------------------" << std::endl;
  auto search_results = found_words.find(boost::to_upper_copy(args[0]));
  if (search_results == found_words.end())
  {
    std::cout << "NONE FOUND" << std::endl;
  }
  else
  {
    std::vector<std::string> address_vec = search_results->second;
    for (const std::string & x : address_vec)
    {
      std::cout << x << std::endl;
    }
  }
  std::cout << "------------------------" << std::endl;
  return true;
}

//--------------------------------------------------------------------------------

bool set_prefix(const std::vector<std::string> &args)
{
  if (args.empty())
  {
    std::stringstream ss;

    ss << "0x" << std::uppercase << std::setfill('0') << std::setw(4) << std::hex << options::address_prefix;
    std::cout << "Current Coin         = " << options::address_prefix_label << "\n"
              << "Current Prefix       = " << options::address_prefix << "\n"
              << "Current Prefix (hex) = " << ss.str() << std::endl;
    return true;
  }

  auto muh_prefix = prefix_map.find(boost::to_upper_copy(args[0]));
  if (muh_prefix == prefix_map.end())
  {
    try{
      options::address_prefix       = boost::lexical_cast<uint64_t>(args[0]);
      options::address_prefix_label = "NA";
    }
    catch(boost::bad_lexical_cast& e){
      fail_msg_writer() << "Invalid prefix choice" << std::endl;
      return true;
    }
  }
  else
  {
    options::address_prefix       = muh_prefix->second;
    options::address_prefix_label = muh_prefix->first;
  }

  success_msg_writer() << "Prefix successfully updated" << std::endl;
  return true;
}

//--------------------------------------------------------------------------------

bool set_params(const std::vector<std::string> &args)
{
  if(args.empty())
  {
    std::cout << "     Min Start Pos: " << options::min_start_pos << "\n"
              << "     Max Start Pos: " << options::max_start_pos << "\n"
              << "Search Word Length: " << options::search_word_length << std::endl;
    return true;
  }
  else if (args.size() < 3)
  {
    fail_msg_writer() << "Expected four parameter values, see 'help' command." << std::endl;
    return true;
  }

  try
  {
    options::min_start_pos      = boost::lexical_cast<int>(args[0]);
    options::max_start_pos      = boost::lexical_cast<int>(args[1]);
    options::search_word_length = boost::lexical_cast<int>(args[2]);
    success_msg_writer() << "Search parameters changed" << std::endl;
    return true;
  }
  catch(boost::bad_lexical_cast& e)
  {
    fail_msg_writer() << "Expected three integers" << std::endl;
    return true;

  }
}

//--------------------------------------------------------------------------------

bool toggle_success_msg(const std::vector<std::string> &args)
{
  options::show_success_msg = !options::show_success_msg;
  success_msg_writer() << "Show message on success is now set to " << (options::show_success_msg ? "TRUE" : "FALSE") << std::endl;
  return true;
}

//--------------------------------------------------------------------------------

bool help(const std::vector<std::string> &args)
{
  //Simple display help function to start with.
  std::stringstream ss;
  ss << "List of all Commands: " << std::endl;
  std::string usage = m_cmd_binder.get_usage();
  boost::replace_all(usage, "\n", "\n  ");
  usage.insert(0, "  ");
  ss << usage << std::endl;
  std::cout << ss.str();

  //std::cout << m_cmd_binder.get_usage();
  return true;
}

//--------------------------------------------------------------------------------

void bind_commands()
{
  m_cmd_binder.set_handler("start"            , boost::bind(&start_search, _1)       , "start <word file> <output file> [num_threads]- start address search");
  m_cmd_binder.set_handler("stop"             , boost::bind(&stop_search, _1)        , "stop - stop address search");
  m_cmd_binder.set_handler("results"          , boost::bind(&show_results, _1)       , "results - [a-z] [0-9] show found words starting with a certain letter and/or greater than a certain length");
  m_cmd_binder.set_handler("show_addresses"   , boost::bind(&show_addresses, _1)     , "show_addresses <word> - show addresses found for <word>");
  m_cmd_binder.set_handler("set_params"       , boost::bind(&set_params, _1)         , "set_params <min start pos> <max start pos> <search word length>");
  m_cmd_binder.set_handler("set_prefix"       , boost::bind(&set_prefix, _1)         , "set_prefix <XMR | XMR_TEST | AEON | number> - Set prefix either to a given number of specify a coin");
  m_cmd_binder.set_handler("show_success_msg" , boost::bind(&toggle_success_msg, _1) , "show_success_msg - toggles whether to show a message when an address is found");
  m_cmd_binder.set_handler("help"             , boost::bind(&help, _1)               , "help - show this help");
  //We don't need an exit command.  Exit is built in.
}

//-----------------------------------------------------------------------------------

int main(int argc, char* argv[])
{
  logo::Monero::display_logo();
  std::cout << "--------------XMR vanity address generator--------------" << std::endl;

  bind_commands();
  m_cmd_binder.run_handling(std::string("[VANITY SEARCH]: "), "");

  //Code below is run when the exit command is given.
  if (search_active)
  {
    std::cout << "Stopping Vanity Search..." << std::endl;
    search_active=false;
    for (size_t i=0;i<search_threads.size(); i++) search_threads[i].join();
    std::cout << "Vanity Search Stopped" << std::endl;
    my_ostream.close();
  }


  return 0;
}
