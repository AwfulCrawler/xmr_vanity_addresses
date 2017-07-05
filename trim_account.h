// Author: AwfulCrawler (2017)
//
// Parts of the cpp file are orignally copyright (c) 2014-2016, The Monero Project
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

#include "crypto/crypto.h"
#include "cryptonote_core/cryptonote_basic.h"
#include "cryptonote_core/cryptonote_format_utils.h"
#include <iostream>
#include <unordered_set>

extern "C" {
   #include "crypto/crypto-ops.h"
   #include "crypto/random.h"
}



uint64_t load_8(const unsigned char *in);
void sc_add1(unsigned char* a);

class trim_account
{
public:
  trim_account()
  {
    random_keys();
  }

  //~trim_account() {}

  void random_keys();
  void increment_keys();
  void derive_keys();
  std::string get_public_address_str(uint64_t a_prefix);
  std::string get_private_spend_key();
  std::string get_private_view_key();
  crypto::secret_key get_raw_private_spend_key();
  bool secret_key_to_public_key(const crypto::secret_key &sec, crypto::public_key &pub);

private:
  cryptonote::account_public_address public_address;
  crypto::secret_key private_spend_key;
  crypto::secret_key private_view_key;
};
