//===- CryptoUtils.cpp - AES-based Pseudo-Random Generator ------------------------===//
/*
  Copyright (C) 2017 Zhang(https://github.com/Naville/)

  Hikari is relicensed from Obfuscator-LLVM and LLVM upstream's permissive NCSA license
  to GNU Affero General Public License Version 3 with exceptions listed below.
  tl;dr: The obfuscated LLVM IR and/or obfuscated binary is not restricted in anyway,
  however any other project containing code from Hikari needs to be open source and licensed under AGPLV3 as well, even for web-based obfuscation services.

  Exceptions:
  Anyone who has associated with ByteDance in anyway at any past, current, future time point is prohibited from direct using this piece of software or create any derivative from it

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU Affero General Public License as published
  by the Free Software Foundation, either version 3 of the License, or
  any later version.
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Affero General Public License for more details.
  You should have received a copy of the GNU Affero General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "llvm/Transforms/Obfuscation/CryptoUtils.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/Twine.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Format.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <chrono>

using namespace llvm;
namespace llvm {
ManagedStatic<CryptoUtils> cryptoutils;
}
CryptoUtils::CryptoUtils() {

}

uint32_t CryptoUtils::scramble32(uint32_t in,std::map<uint32_t/*IDX*/,uint32_t/*VAL*/>& VMap) {
  if(VMap.find(in)==VMap.end()){
    uint32_t V=get_uint32_t();
    VMap[in]=V;
    return V;
  }
  else{
    return VMap[in];
  }
}
CryptoUtils::~CryptoUtils() {
  if(eng!=nullptr){
    delete eng;
  }
}
void CryptoUtils::prng_seed(){
  using namespace std::chrono;
  std::uint_fast64_t ms = duration_cast< milliseconds >(system_clock::now().time_since_epoch()).count();
  errs()<<format("std::mt19937_64 seeded with current timestamp: %" PRIu64"",ms)<<"\n";
  eng=new std::mt19937_64(ms);
}
void CryptoUtils::prng_seed(std::uint_fast64_t seed){
  errs()<<format("std::mt19937_64 seeded with: %" PRIu64"",seed)<<"\n";
  eng=new std::mt19937_64(seed);
}
std::uint_fast64_t CryptoUtils::get_raw(){
  if(eng==nullptr){
    prng_seed();
  }
  return (*eng)();
}
uint32_t CryptoUtils::get_range(uint32_t min,uint32_t max) {
  if(max==0){
    return 0;
  }
  std::uniform_int_distribution<uint32_t> dis(min, max-1);
  return dis(*eng);
}
