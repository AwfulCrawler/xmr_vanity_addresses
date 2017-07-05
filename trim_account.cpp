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

#include "trim_account.h"
#include "common/base58.h"
#include "string_tools.h"
#include <boost/thread/mutex.hpp>
#include <boost/thread/lock_guard.hpp>

boost::mutex my_random_lock;


using namespace crypto;

static inline unsigned char *operator &(ec_point &point) {
  return &reinterpret_cast<unsigned char &>(point);
}

static inline const unsigned char *operator &(const ec_point &point) {
  return &reinterpret_cast<const unsigned char &>(point);
}

static inline unsigned char *operator &(ec_scalar &scalar) {
  return &reinterpret_cast<unsigned char &>(scalar);
}

static inline const unsigned char *operator &(const ec_scalar &scalar) {
  return &reinterpret_cast<const unsigned char &>(scalar);
}

static inline void random_scalar(ec_scalar &res) {
  unsigned char tmp[64];
  generate_random_bytes_not_thread_safe(64, tmp);
  sc_reduce(tmp);
  memcpy(&res, tmp, 32);
}

//--------------------------------------------------------------------------------
uint64_t load_8(const unsigned char *in)
{
  uint64_t result;
  result = (uint64_t) in[0];
  result |= ((uint64_t) in[1]) << 8;
  result |= ((uint64_t) in[2]) << 16;
  result |= ((uint64_t) in[3]) << 24;
  result |= ((uint64_t) in[4]) << 32;
  result |= ((uint64_t) in[5]) << 40;
  result |= ((uint64_t) in[6]) << 48;
  result |= ((uint64_t) in[7]) << 56;
  return result;
}

void sc_add1(unsigned char* a)
{
  int64_t a0 = load_8(a);
  a0 += 1;
  a[0] = a0 >> 0;
  a[1] = a0 >> 8;
  a[2] = a0 >> 16;
  a[3] = a0 >> 24;
  a[4] = a0 >> 32;
  a[5] = a0 >> 40;
  a[6] = a0 >> 48;
  a[7] = a0 >> 56;
}


//------------------------------------------------------------------------------
//
//trim_account functions.  The only things that trim_account has are
//private and public keys and functions for generating them.
//
//------------------------------------------------------------------------------
void trim_account::random_keys(){
  boost::lock_guard<boost::mutex> lock(my_random_lock);
  //random_scalar ends up using generate_random_bytes_not_thread_safe
  //from random.c in crypto directory.
  random_scalar(private_spend_key);
  sc_reduce32(&private_spend_key); //In crypto-ops.c/h
  derive_keys();
}
//--------------------------------------------------------------------------------
void trim_account::increment_keys(){
  sc_add1(&private_spend_key);
  sc_reduce32(&private_spend_key);
  derive_keys();
}
//--------------------------------------------------------------------------------
void trim_account::derive_keys(){
  keccak((uint8_t *)&private_spend_key, sizeof(secret_key), (uint8_t *)&private_view_key, sizeof(secret_key));   //In keccak.c/h
  sc_reduce32(&private_view_key);
  secret_key_to_public_key(private_spend_key, public_address.m_spend_public_key); //in crypto.c/h but copied below...only need crypto-ops.c/h
  secret_key_to_public_key(private_view_key, public_address.m_view_public_key);
}
//--------------------------------------------------------------------------------
bool trim_account::secret_key_to_public_key(const secret_key &sec, public_key &pub) {
  ge_p3 point;
  if (sc_check(&sec) != 0) {
    return false;
  }
  ge_scalarmult_base(&point, &sec);
  ge_p3_tobytes(&pub, &point);
  return true;
}
//--------------------------------------------------------------------------------
std::string trim_account::get_public_address_str(uint64_t a_prefix){
  return tools::base58::encode_addr(a_prefix, t_serializable_object_to_blob(public_address));
}
//--------------------------------------------------------------------------------
std::string trim_account::get_private_spend_key(){
  return epee::string_tools::pod_to_hex(private_spend_key);
}
//--------------------------------------------------------------------------------
crypto::secret_key trim_account::get_raw_private_spend_key(){
  return private_spend_key;
}
//--------------------------------------------------------------------------------
std::string trim_account::get_private_view_key(){
  return epee::string_tools::pod_to_hex(private_view_key);
}
